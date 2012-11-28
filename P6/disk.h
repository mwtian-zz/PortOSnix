#ifndef __DISK_H__
#define __DISK_H__

/*
 * disk.h
 *       This module simulates a SCSI harddrive
 */

/* get the correct version of Linux recognized */
#include <stdio.h>
#include <pthread.h>
#include <semaphore.h>
#include "defs.h"
#include "interrupts.h"

#define DISK_INTERRUPT_TYPE 4

#define DISK_BLOCK_SIZE 4096
#define MAX_PENDING_DISK_REQUESTS 128

/* global variables that control the behavior of the disk */
extern double crash_rate;
extern double failure_rate;
extern double reordering_rate;

/* parameters to control startup behavior. To create a new disk, set use_existing_disk
   to 0, specify the disk_name, disk_flags and disk_size, then call disk_initialize().
   To startup an existing disk, set use_existing_disk to 1 and specify the disk_name,
   then call disk_initialize. Upon successful startup, disk_flags and disk_size will
   be correctly initialized.
 */
extern int use_existing_disk;	/* Set to 1 if using an existing disk, 0 to create a new one */
extern const char* disk_name;	/* Linux filename that stores your virtual disk */
extern int disk_flags;			/* Set to DISK_READWRITE or DISK_READONLY */
extern int disk_size;			/* Set to the number of blocks allocated for disk */

typedef enum {
    DISK_READWRITE=0,
    DISK_READONLY=1
} disk_flags_t;

typedef struct {
    int size;    /* in blocks */
    int flags;
} disk_layout_t;

typedef enum { DISK_RESET, /* required after disk crash */
               DISK_SHUTDOWN,
               DISK_READ,
               DISK_WRITE
             } disk_request_type_t ;

typedef enum { DISK_REPLY_OK,
               DISK_REPLY_FAILED, /* disk failed on this request for no apparent reason */
               DISK_REPLY_ERROR, /* disk nonexistent or block outside disk requested */
               DISK_REPLY_CRASHED
             } disk_reply_t;

/*
 * Datastructure used to make and receive replies to disk
 * requests.
*/

typedef struct {
    int blocknum;
    char* buffer; /* pointer to the memory buffer */
    disk_request_type_t type; /* type of disk request */
} disk_request_t;

typedef struct disk_queue_elem_t {
    disk_request_t request;
    struct disk_queue_elem_t* next;
} disk_queue_elem_t ;

/* Do not modify by hand elements of type disk_t since synchronization
   is required
*/

typedef struct {
    disk_layout_t layout;
    FILE* file;
    disk_queue_elem_t* queue;
    disk_queue_elem_t* last;
    sem_t semaphore; /* the semaphore should be signaled when something is added to the queue */
} disk_t;

/* structure used to pass arguments through interrupts */
typedef struct {
    disk_t* disk;
    disk_request_t request;
    disk_reply_t reply;
} disk_interrupt_arg_t;


typedef void (*disk_handler_t)(disk_t* disk, disk_request_t, disk_reply_t);

int
disk_initialize(disk_t* disk);

int
disk_send_request(disk_t*, int, char*,disk_request_type_t);

int
disk_shutdown(disk_t* disk);

void
disk_set_flags(disk_t* disk, int flags);

void
disk_unset_flags(disk_t* disk, int flags);

int
disk_read_block(disk_t* disk, int blocknum, char* buffer);

int
disk_write_block(disk_t* disk, int blocknum, char* buffer);

void
install_disk_handler(interrupt_handler_t disk_handler);

#endif /*__DISK_H__*/
