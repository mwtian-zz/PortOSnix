#ifndef __SYNCH_H__
#define __SYNCH_H__

/*
 * Definitions for high-level synchronization primitives.
 *
 *	You must implement the procedures and types defined in this interface.
 */

typedef struct semaphore *semaphore_t;


/*
 * Semaphores.
 */

/*
 * semaphore_t semaphore_create()
 *	Allocate a new semaphore.
 */
extern semaphore_t semaphore_create();

/*
 * semaphore_destroy(semaphore_t sem);
 *	Deallocate a semaphore.
 */
extern void semaphore_destroy(semaphore_t sem);
 
/*
 * semaphore_initialize(semaphore_t sem, int cnt)
 *	initialize the semaphore data structure pointed at by
 *	sem with an initial value cnt.
 */
extern void semaphore_initialize(semaphore_t sem, int cnt);

/*
 * semaphore_P(semaphore_t sem)
 *	P on the sempahore. Your new implementation should use TAS locks.
 */
extern void semaphore_P(semaphore_t sem);

/*
 * semaphore_V(semaphore_t sem)
 *	V on the sempahore. Your new implementation should use TAS locks.
 */
extern void semaphore_V(semaphore_t sem);


#endif /*__SYNCH_H__*/
