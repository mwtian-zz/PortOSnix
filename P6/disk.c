#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "defs.h"
#include "disk.h"
#include "interrupts_private.h"
#include "random.h"

pthread_mutex_t disk_mutex;

/* disk operation parameters */
double crash_rate = 0.0;
double failure_rate = 0.0;
double reordering_rate = 0.0;

/* parameters to control startup behavior. To create a new disk, set use_existing_disk
   to 0, specify the disk_name, disk_flags and disk_size, then call disk_initialize().
   To startup an existing disk, set use_existing_disk to 1 and specify the disk_name,
   then call disk_initialize. Upon successful startup, disk_flags and disk_size will
   be correctly initialized.
 */

/* Set to 1 if using an existing disk, 0 to create a new one */
int use_existing_disk = 1;
/* Linux filename that stores your virtual disk */
const char* disk_name = "minidisk";
/* Set to DISK_READWRITE or DISK_READONLY */
int disk_flags = DISK_READWRITE;
/* Set to the number of blocks allocated for disk */
int disk_size = 128;

void start_disk_poll(disk_t* disk); /* forward declaration */

int disk_send_request(disk_t* disk, int blocknum, char* buffer,
                      disk_request_type_t type)
{
    disk_queue_elem_t* saved_last=NULL;
    disk_queue_elem_t* disk_request
    = (disk_queue_elem_t*) malloc(sizeof(disk_queue_elem_t));

    if (disk_request == NULL)
        return -1;

    /* put data in the request */
    disk_request->request.blocknum = blocknum;
    disk_request->request.buffer = buffer;
    disk_request->request.type = type;

    /* queue the request */
    pthread_mutex_lock(&disk_mutex);

    if (type == DISK_SHUTDOWN) {
        /* optimistically put the request on top of the queue */
        disk_request->next=disk->queue;
        disk->queue=disk_request;
    } else {
        /* optimistically put it at the end */
        disk_request->next=NULL;
        saved_last=disk->last;
        if (disk->last != NULL) {
            disk->last->next=disk_request;
        } else {
            disk->queue=disk_request;
        }
        disk->last=disk_request;
    }

    /* signal the task that simulates the disk */
    if (sem_post(&disk->semaphore)) {
        kprintf("You have exceeded the maximum number of requests pending.\n");

        /* undo changes made to the request queue */
        if (type == DISK_SHUTDOWN)
            disk->queue=disk->queue->next;
        else {
            disk->last=saved_last;
            if (disk->last != NULL)
                disk->last->next=NULL;
            else
                disk->queue=NULL;
        }
        free(disk_request);

        pthread_mutex_unlock(&disk_mutex);
        return -2;
    }

    pthread_mutex_unlock(&disk_mutex);

    return 0;
}


static int
disk_create(disk_t* disk, const char* name, int size, int flags)
{
    if ((disk->file = fopen(name, "w+b")) == NULL)
        return -1;
    disk->layout.size = size;
    disk->layout.flags = flags;

    /* write the disk layout to the "disk" */
    if (fwrite(&disk->layout, 1, sizeof(disk_layout_t), disk->file)
            != sizeof(disk_layout_t)) {
        fclose(disk->file);
        return -1;
    }

    start_disk_poll((void*)disk);

    return 0;
}

static int
disk_startup(disk_t* disk, const char* name)
{
    if ((disk->file = fopen(name, "r+b")) == NULL)
        return -1;

    if (fread(&disk->layout, 1, sizeof(disk_layout_t), disk->file) !=
            sizeof(disk_layout_t)) {
        fclose(disk->file);
        return -1;
    }

    start_disk_poll((void*)disk);

    return 0;
}

int disk_initialize(disk_t* disk)
{
    int result=0;

    /* create new disk or startup existing disk */
    if (use_existing_disk) {
        result = disk_startup(disk, disk_name);
        if (result == 0) {
            disk_size = disk->layout.size;
            disk_flags = disk->layout.flags;
        } else {
            disk_size = 0;
            disk_flags = DISK_READONLY;
        }
    } else {
        result = disk_create(disk, disk_name, disk_size, disk_flags);
    }

    return result;
}

static void
disk_write_layout(disk_t* disk)
{
    if (fseek(disk->file, 0, SEEK_SET) != 0)
        fclose(disk->file);
    else if (fwrite(&disk->layout, 1, sizeof(disk_layout_t), disk->file) !=
             sizeof(disk_layout_t))
        fclose(disk->file);
    else
        fflush(disk->file);
}

int
disk_shutdown(disk_t* disk)
{
    disk_send_request(disk, 0, NULL, DISK_SHUTDOWN);
    return 0;
}

void
disk_set_flags(disk_t* disk, int flags)
{
    pthread_mutex_lock(&disk_mutex);

    disk->layout.flags |= flags;
    disk_write_layout(disk);

    pthread_mutex_unlock(&disk_mutex);
}

void
disk_unset_flags(disk_t* disk, int flags)
{
    pthread_mutex_lock(&disk_mutex);

    disk->layout.flags ^= disk->layout.flags | flags;
    disk_write_layout(disk);

    pthread_mutex_unlock(&disk_mutex);
}

int
disk_read_block(disk_t* disk, int blocknum, char* buffer)
{
    return
        disk_send_request(disk,blocknum,buffer,DISK_READ);
}

int
disk_write_block(disk_t* disk, int blocknum, char* buffer)
{
    return
        disk_send_request(disk,blocknum,buffer,DISK_WRITE);
}


/* handle read and write to disk. Watch the job queue and
   submit the requests to the operating system one by one.
   On completion, the user suplied function with the user
   suplied parameter is called

   The interrupt mechanism is used much like in the network
   case. One such process per disk.

   The argument is a disk_t with the disk description.
 */
void disk_poll(void* arg)
{
    enum { DISK_OK, DISK_CRASHED } disk_state;

    disk_t* disk = (disk_t*) arg;
    disk_layout_t layout;

    disk_interrupt_arg_t* disk_interrupt;
    disk_queue_elem_t* disk_request;

    disk_state=DISK_OK;

    for (;;) {
        int blocknum;
        char* buffer;
        disk_request_type_t type;
        int offset;

        /* We use mutex to protect queue handling, as should
           the code that inserts requests in the queue
         */

        /* wait for somebody to put something in the queue */
        sem_wait(&disk->semaphore);

        if (DEBUG)
            kprintf("Disk Controler: got a request.\n");

        /* get exclusive access to queue handling  and dequeue a request */
        pthread_mutex_lock(&disk_mutex);

        layout = disk->layout; /* this is the layout used until the request is fulfilled */
        /* this is safe since a disk can only grow */

        if (disk->queue != NULL) {
            disk_interrupt = (disk_interrupt_arg_t*)
                             malloc(sizeof(disk_interrupt_arg_t));
            assert( disk_interrupt != NULL);

            disk_interrupt->disk = disk;
            /* we look first at the first request in the queue
               to see if it is special.
             */
            disk_interrupt->request =
                disk->queue->request;

            /* check if we shut down the disk */
            if (disk->queue->request.type == DISK_SHUTDOWN) {
                if (DEBUG)
                    kprintf("Disk: Shutting down.\n");

                disk_interrupt->reply=DISK_REPLY_OK;
                fclose(disk->file);
                pthread_mutex_unlock(&disk_mutex);
                sem_destroy(&disk->semaphore);
                send_interrupt(DISK_INTERRUPT_TYPE, mini_disk_handler, (void*)disk_interrupt);
                break; /* end the disk task */
            }

            /* check if we got to reset the disk */
            if (disk->queue->request.type == DISK_RESET) {
                disk_queue_elem_t* curr;
                disk_queue_elem_t *next;

                if (DEBUG)
                    kprintf("Disk: Resetting.\n");

                disk_interrupt->reply=DISK_REPLY_OK;
                /* empty the queue */
                curr=disk->queue;
                while (curr!=NULL) {
                    next=curr->next;
                    free(curr);
                    curr=next;
                }
                disk->queue = disk->last = NULL;

                disk_state = DISK_OK;
                pthread_mutex_unlock(&disk_mutex);
                goto sendinterrupt;
            }

            /* permute the first two elements in the queue
               probabilistically if queue has two elements
             */
            if (disk->queue->next !=NULL &&
                    (genrand() < reordering_rate)) {
                disk_queue_elem_t* first = disk->queue;
                disk_queue_elem_t* second = first->next;
                first->next = second->next;
                second->next = first;
                disk->queue = second;
                if (disk->last == second)
                    disk->last = first;
            }

            /* dequeue the first request */
            disk_request = disk->queue;
            disk->queue = disk_request->next;
            if (disk->queue == NULL)
                disk->last = NULL;
        } else {
            /* empty queue, release the lock and leave */
            pthread_mutex_unlock(&disk_mutex);
            break;
        }

        pthread_mutex_unlock(&disk_mutex);

        disk_interrupt->request = disk_request->request;

        /* crash the disk ocasionally */
        if (genrand() < crash_rate) {
            disk_state = DISK_CRASHED;

            /*      if (DEBUG) */
            kprintf("Disk: Crashing disk.\n");

        }

        /* check if disk crashed */
        if (disk_state == DISK_CRASHED) {
            disk_interrupt->reply=DISK_REPLY_CRASHED;
            goto sendinterrupt;
        }

        if ( genrand() < failure_rate ) {
            /* Trash the request */
            disk_interrupt->reply = DISK_REPLY_FAILED;

            if (DEBUG)
                kprintf("Disk: Request failed.\n");

            goto sendinterrupt;
        }

        /* Check validity of request */

        disk_interrupt->reply = DISK_REPLY_OK;

        blocknum = disk_request->request.blocknum;
        buffer = disk_request->request.buffer;
        type = disk_request->request.type;

        if (DEBUG)
            kprintf("Disk Controler: got a request for block %d type %d .\n",
                    blocknum, type);

        /* If we got here is a read or a write request */

        offset = DISK_BLOCK_SIZE*(blocknum + 1);

        if ( (blocknum >= layout.size) ||
                (fseek(disk->file, offset, SEEK_SET) != 0) ) {
            disk_interrupt->reply = DISK_REPLY_ERROR;

            if (DEBUG)
                kprintf("Disk Controler: Block too big or failed fseek, block=%d,  offset=%d, disk_size=%d.\n",
                        blocknum, offset, layout.size);

            goto sendinterrupt;
        }

        switch (type) {
        case DISK_READ:
            if (fread(buffer, 1, DISK_BLOCK_SIZE, disk->file)
                    < DISK_BLOCK_SIZE)
                disk_interrupt->reply = DISK_REPLY_ERROR;
            if (DEBUG)
                kprintf("Disk: Read request.\n");

            break;
        case DISK_WRITE:
            if (fwrite(buffer, 1, DISK_BLOCK_SIZE, disk->file)
                    < DISK_BLOCK_SIZE)
                disk_interrupt->reply = DISK_REPLY_ERROR;
            fflush(disk->file);
            if (DEBUG)
                kprintf("Disk: Write request.\n");

            break;
        default:
            break;
        }

sendinterrupt:
        if (DEBUG)
            kprintf("Disk Controler: sending an interrupt for block %d, request type %d, with reply %d.\n",
                    disk_interrupt->request.blocknum,
                    disk_interrupt->request.type,
                    disk_interrupt->reply);

        send_interrupt(DISK_INTERRUPT_TYPE, mini_disk_handler, (void*)disk_interrupt);
    }

}

void start_disk_poll(disk_t* disk)
{
    pthread_t disk_thread;
    sigset_t set;
    struct sigaction sa;
    sigset_t old_set;
    sigemptyset(&set);
    sigaddset(&set,SIGRTMAX-1);
    sigaddset(&set,SIGRTMAX-2);
    sigprocmask(SIG_BLOCK,&set,&old_set);

    sa.sa_handler = (void*)handle_interrupt;
    sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
    sa.sa_sigaction= (void*)handle_interrupt;
    sigemptyset(&sa.sa_mask);
    sigaddset(&sa.sa_mask,SIGRTMAX-2);
    sigaddset(&sa.sa_mask,SIGRTMAX-1);
    if (sigaction(SIGRTMAX-2, &sa, NULL) == -1)
        AbortOnError(0);


    /* reset the request queue */
    disk->queue = disk->last = NULL;

    /* create request semaphore */
    AbortOnCondition(sem_init(&disk->semaphore, 0, 0),"sem_init");

    AbortOnCondition(pthread_create(&disk_thread, NULL, (void*)disk_poll, (void*)disk),
                     "pthread");

    pthread_sigmask(SIG_SETMASK,&old_set,NULL);
}

void install_disk_handler(interrupt_handler_t disk_handler)
{
    kprintf("Starting disk interrupt.\n");
    mini_disk_handler = disk_handler;

    /* create mutex used to protect disk datastructures */
    AbortOnCondition(pthread_mutex_init(&disk_mutex, NULL),"mutex");
}
