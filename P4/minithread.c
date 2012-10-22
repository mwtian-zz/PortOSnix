/*
 * minithread.c:
 *	This file provides a few function headers for the procedures that
 *	you are required to implement for the minithread assignment.
 *
 *	EXCEPT WHERE NOTED YOUR IMPLEMENTATION MUST CONFORM TO THE
 *	NAMING AND TYPING OF THESE PROCEDURES.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include "interrupts.h"
#include "minithread.h"
#include "queue.h"
#include "synch.h"

#include <assert.h>

/*
 * A minithread should be defined either in this file or in a private
 * header file.  Minithreads have a stack pointer with to make procedure
 * calls, a stackbase which points to the bottom of the procedure
 * call stack, the ability to be enqueueed and dequeued, and any other state
 * that you feel they must have.
 */


/* minithread functions */

minithread_t minithread_fork(proc_t proc, arg_t arg)
{
	return NULL;
}

minithread_t minithread_create(proc_t proc, arg_t arg)
{
	return NULL;
}

minithread_t minithread_self()
{
	return NULL;
}

int minithread_id()
{
	return 0;
}

/* DEPRECATED. Beginning from project 2, you should use minithread_unlock_and_stop() instead
 * of this function.
 */
void minithread_stop()
{

}

void minithread_start(minithread_t t)
{

}

void minithread_yield()
{
}

/*
 * This is the clock interrupt handling routine.
 * You have to call minithread_clock_init with this
 * function as parameter in minithread_system_initialize
 */
void clock_handler(void* arg)
{

}

/*
 * Initialization.
 *
 * 	minithread_system_initialize:
 *	 This procedure should be called from your C main procedure
 *	 to turn a single threaded UNIX process into a multithreaded
 *	 program.
 *
 *	 Initialize any private data structures.
 * 	 Create the idle thread.
 *       Fork the thread which should call mainproc(mainarg)
 * 	 Start scheduling.
 *
 */
void minithread_system_initialize(proc_t mainproc, arg_t mainarg)
{

}

/*
 * minithread_unlock_and_stop(tas_lock_t* lock)
 *	Atomically release the specified test-and-set lock and
 *	block the calling thread.
 */
void minithread_unlock_and_stop(tas_lock_t* lock)
{

}

/*
 * sleep with timeout in milliseconds
 */
void minithread_sleep_with_timeout(int delay)
{

}
