#include <assert.h>
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
struct semaphore {
	int count;
	/* tas_lock_t lock; */
	queue_t wait;
};


/*
 * semaphore_t semaphore_create()
 *	Allocate a new semaphore.
 */
semaphore_t
semaphore_create() {
	semaphore_t sem = malloc(sizeof(*sem));
	return sem;
}

/*
 * semaphore_destroy(semaphore_t sem);
 *	Deallocate a semaphore.
 */
void
semaphore_destroy(semaphore_t sem) {
    if (NULL == sem)
        return;
	free(sem->wait);
	free(sem);
}

/*
 * semaphore_initialize(semaphore_t sem, int cnt)
 *	initialize the semaphore data structure pointed at by
 *	sem with an initial value cnt.
 */
void
semaphore_initialize(semaphore_t sem, int cnt) {
    queue_t q;
    if (NULL == sem)
        return;
    q = queue_new();
    if (NULL == q)
        return;
	sem->count = cnt;
	/* sem->lock = 0; */
	sem->wait = q;
}

/*
 * semaphore_P(semaphore_t sem)
 *	P on the sempahore.
 */
void
semaphore_P(semaphore_t sem) {
	/* while (1 == atomic_test_and_set(&(sem->lock)))
		; */
	if (0 > --(sem->count)) {
	    queue_append(sem->wait,(void*)minithread_self());
	    /* atomic_clear(&(sem->lock)); */
		minithread_stop();

	} /* else {
        atomic_clear(&(sem->lock));
    } */
}

/*
 * semaphore_V(semaphore_t sem)
 *	V on the sempahore.
 */
void
semaphore_V(semaphore_t sem) {
	minithread_t t;
	/* while (1 == atomic_test_and_set(&(sem->lock)))
		; */
	if (0 >= ++(sem->count)) {
		queue_dequeue(sem->wait,(void**)&t);
		/* atomic_clear(&(sem->lock)); */
		minithread_start(t);
	} /* else {
        atomic_clear(&(sem->lock));
    } */
}
