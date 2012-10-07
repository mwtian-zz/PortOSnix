#include <stdio.h>
#include <stdlib.h>

#include "defs.h"
#include "synch.h"
#include "queue.h"
#include "minithread.h"

/*
 *	You must implement the procedures and types defined in this interface.
 */

/*
 * Semaphores.
 */
struct semaphore
{
    /* This is temporary so that the compiler does not error on an empty struct.
     * You should replace this with your own struct members.
     */
    int tmp;
};

/*
 * semaphore_t semaphore_create()
 *	Allocate a new semaphore.
 */
semaphore_t semaphore_create()
{

	return NULL;
}

/*
 * semaphore_destroy(semaphore_t sem);
 *	Deallocate a semaphore.
 */
void semaphore_destroy(semaphore_t sem)
{

}
 
/*
 * semaphore_initialize(semaphore_t sem, int cnt)
 *	initialize the semaphore data structure pointed at by
 *	sem with an initial value cnt.
 */
void semaphore_initialize(semaphore_t sem, int cnt)
{

}

/*
 * semaphore_P(semaphore_t sem)
 *	P on the sempahore. Your new implementation should use TAS locks.
 */
void semaphore_P(semaphore_t sem)
{

}

/*
 * semaphore_V(semaphore_t sem)
 *	V on the sempahore. Your new implementation should use TAS locks.
 */
void semaphore_V(semaphore_t sem)
{

}
