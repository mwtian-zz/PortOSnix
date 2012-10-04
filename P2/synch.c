#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "defs.h"
#include "queue.h"
#include "minithread.h"
#include "interrupts.h"
#include "synch.h"

/*
 *    You must implement the procedures and types defined in this interface.
 */

/*
 * Semaphores.
 */
struct semaphore {
    int count;
    tas_lock_t lock;
    queue_t wait;
};


/*
 * semaphore_t semaphore_create()
 *    Allocate a new semaphore.
 */
semaphore_t
semaphore_create()
{
    queue_t q;
    semaphore_t sem = malloc(sizeof(struct semaphore));
    if (NULL == sem) {
        return NULL;
    }
    q = queue_new();
    if (NULL == q) {
        free(sem);
        return NULL;
    }
    sem->wait = q;
    return sem;
}

/*
 * semaphore_destroy(semaphore_t sem);
 *    Deallocate a semaphore.
 */
void
semaphore_destroy(semaphore_t sem)
{
    if (NULL == sem)
        return;
    if (NULL != sem->wait)
        free(sem->wait);
    free(sem);
}

/*
 * semaphore_initialize(semaphore_t sem, int cnt)
 *    initialize the semaphore data structure pointed at by
 *    sem with an initial value cnt.
 */
void
semaphore_initialize(semaphore_t sem, int cnt)
{
    if (NULL == sem)
        return;
    sem->count = cnt;
    sem->lock = 0;
}

/*
 * semaphore_P(semaphore_t sem)
 *    P on the sempahore.
 */
void
semaphore_P(semaphore_t sem)
{
    while (atomic_test_and_set(&(sem->lock)) == 1)
        ;
    if (--(sem->count) < 0) {
        queue_append(sem->wait, (void*) minithread_self());
        minithread_unlock_and_stop(&(sem->lock));
    } else {
        atomic_clear(&(sem->lock));
    }
}

/*
 * semaphore_V(semaphore_t sem)
 *    V on the sempahore.
 */
void
semaphore_V(semaphore_t sem)
{
    minithread_t t;
    while (atomic_test_and_set(&(sem->lock)) == 1)
        ;
    if (++(sem->count) <= 0) {
        if (queue_dequeue(sem->wait, (void**) &t) == 0)
            minithread_start(t);
    }
    atomic_clear(&(sem->lock));
}
