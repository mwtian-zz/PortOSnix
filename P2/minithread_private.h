/*
 * Internal data structure for the thread control block
 */

#ifndef __MINITHREAD_STRUCT_H__
#define __MINITHREAD_STRUCT_H__

#include "queue_private.h"
#include "multilevel_queue.h"
#include "minithread.h"

/* Maximium priority level. Minimum is 0. */
#define MAX_PRIORITY 3

/*
 * Define constants for thread status.
 */
enum status {
    INITIAL,
    RUNNING,
    READY,
    BLOCKED,
    EXITED
};

/*
 * struct minithread:
 * qnode: enable minithread to be enqueued and dequeued.
 * id: thread id.
 * top: stack pointer, points to the top of the stack.
 * base: points to the base of the stack.
 * status: current status.
 */
struct minithread {
    struct node qnode;
    unsigned int id;
    stack_pointer_t top;
    stack_pointer_t base;
    enum status status;
    int priority;
};

/*
 * Information for thread management.
 */
struct thread_monitor {
    unsigned int count;
    unsigned int tidcount;
    minithread_t instack;
    multilevel_queue_t ready;
    queue_t exited;
    long expire;
    int quanta_lim[MAX_PRIORITY + 1];
    long alarm;  /* Nearest alarm ticks to fire? */
};

#endif /*__MINITHREAD_PRIVATE_H__*/
