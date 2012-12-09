#include "defs.h"

#include "disk.h"
#include "minifile.h"
#include "minifile_diskutil.h"

#include "minithread.h"
#include "synch.h"

#define BUFFER_SIZE 65536
char file_create[] = "createfiletest";
char file_open[] = "oepnfiletest";
char file_read[] = "readfiletest";

char buf[BUFFER_SIZE];
int num_threads = 20;
semaphore_t barrier_sem;

int
read_test(int *arg)
{
    int i;
    char local_buf[BUFFER_SIZE];
    char sig = 0;
    minifile_t file = minifile_open(file_read, "r");
    printf("read-test reading.\n");
    minifile_read(file, local_buf, BUFFER_SIZE);
    for (i = 0; i < BUFFER_SIZE; ++i) {
        if (local_buf[i] != (i & 127)) {
            printf("Error at byte %d: %d.\n", i, local_buf[i]);
            sig = 1;
            break;
        }
    }
    if (0 == sig) {
        printf("All bytes are correct.\n");
    } else {
        printf("Error detected.\n");
    }
    minifile_close(file);
    semaphore_V(barrier_sem);
    printf("read-test finishes.\n");
    return 0;
}

int
open_test(int *arg)
{
    minifile_t file = minifile_open(file_open, "w");
    printf("open-test writing.\n");
    minifile_write(file,buf,BUFFER_SIZE);
    minifile_close(file);
    semaphore_V(barrier_sem);
    printf("open-test finishes.\n");
    return 0;
}

int
create_test(int *arg)
{
    return 0;
}

int
file_test(int *arg)
{
    int i;
    minifile_t file;
    printf("Initializing.\n");
    barrier_sem = semaphore_new(0);
    for (i = 0; i < BUFFER_SIZE; ++i) {
        buf[i] = i & 127;
    }
    file = minifile_open(file_read, "w+");
    minifile_write(file,buf,BUFFER_SIZE);
    minifile_close(file);

    printf("Forking read-test.\n");
    for (i = 0; i < num_threads; ++i) {
        minithread_fork(read_test, NULL);
    }
    for (i = 0; i < num_threads; ++i) {
        semaphore_P(barrier_sem);
    }

    /*
    printf("Forking open-test.\n");
    for (i = 0; i < num_threads; ++i) {
        minithread_fork(open_test, NULL);
    }
    for (i = 0; i < num_threads; ++i) {
        semaphore_P(barrier_sem);
    }
    */
    minifile_fsck(NULL);

    return 0;
}

int
main(int argc, char** argv)
{
    use_existing_disk = 1;
    disk_name = "minidisk";
    minithread_system_initialize(file_test, NULL);

    return 0;
}
