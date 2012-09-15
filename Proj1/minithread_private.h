/*
 * Internal data structure for the thread control block
 */

#ifndef __MINITHREAD_STRUCT_H__
#define __MINITHREAD_STRUCT_H__

#include "queue.h"
#include "minithread.h"

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
 * prev: the previous thread in queue.
 * next: the next thread in queue.
 * queue: pointer to the struct queue.
 * id: thread id.
 * top: stack pointer, points to the top of the stack.
 * base: points to the base of the stack.
 * status: current status.
 */
struct minithread {
	struct minithread *prev;
	struct minithread *next;
	struct queue *queue;
	int id;
	stack_pointer_t top;
	stack_pointer_t base;
	enum status status;
};


/*
 * Information for thread managing.
 */
struct thread_monitor {
	int count;
	int tidcount;
	minithread_t instack;
	queue_t ready;
	queue_t exited;
};


#endif /*__MINITHREAD_PRIVATE_H__*/
