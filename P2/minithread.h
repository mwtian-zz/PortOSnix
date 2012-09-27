#ifndef __MINITHREAD_H__
#define __MINITHREAD_H__

#include "machineprimitives.h"
#include "alarm_queue.h"

/*
 * minithread.h:
 *	Definitions for minithreads.
 *
 *	Your assignment is to implement the functions defined in this file.
 *	You must use the names for types and procedures that are used here.
 *	Your procedures must take the exact arguments that are specified
 *	in the comments.  See minithread.c for prototypes.
 */

/* Alarm queue */
extern alarm_queue_t alarm_queue;
/* thread monitor semaphore */
extern semaphore_t wakeup_sem; 
/* alarm id semaphore */
extern semaphore_t alarm_id_sem;       

/*
 * struct minithread:
 *	This is the key data structure for the thread management package.
 *	You must define the thread control block as a struct minithread.
 */

typedef struct minithread *minithread_t;

/*
 * minithread_t
 * minithread_fork(proc_t proc, arg_t arg)
 *	Create and schedule a new thread of control so
 *	that it starts executing inside proc_t with
 *	initial argument arg.
 */	
extern minithread_t minithread_fork(proc_t proc, arg_t arg);


/*
 * minithread_t
 * minithread_create(proc_t proc, arg_t arg)
 *	Like minithread_fork, only returned thread is not scheduled
 *	for execution.
 */
extern minithread_t minithread_create(proc_t proc, arg_t arg);



/*
 * minithread_t minithread_self():
 *	Return identity (minithread_t) of caller thread.
 */
extern minithread_t minithread_self();


/*
 * int minithread_id():
 *      Return thread identifier of caller thread, for debugging.
 *
 */
extern int minithread_id();


/*
 * minithread_stop()
 *	Block the calling thread.
 */
extern void minithread_stop();

/*
 * minithread_start(minithread_t t)
 *	Make t runnable.
 */
extern void minithread_start(minithread_t t);

/*
 * minithread_yield()
 *	Forces the caller to relinquish the processor and be put to the end of
 *	the ready queue.  Allows another thread to run.
 */
extern void minithread_yield();

/*
 * minithread_system_initialize(proc_t mainproc, arg_t mainarg)
 *	Initialize the system to run the first minithread at
 *	mainproc(mainarg).  This procedure should be called from your
 *	main program with the callback procedure and argument specified
 *	as arguments.
 */
extern void minithread_system_initialize(proc_t mainproc, arg_t mainarg);

/*
 * You do not need to implement the following procedure for part 1 of
 * the assignment.  It is required for the preemptive version of the
 * threads package a la part 2. */

/*
 * minithread_unlock_and_stop(tas_lock_t* lock)
 *	Atomically release the specified test-and-set lock and
 *	block the calling thread.
 */
extern void minithread_unlock_and_stop(tas_lock_t* lock);

/*
 * sleep with timeout in microseconds
 */
extern void minithread_sleep_with_timeout(int delay);


#endif /*__MINITHREAD_H__*/

