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
semaphore_create() {
    semaphore_t sem = malloc(sizeof(*sem));
    return sem;
}

/*
 * semaphore_destroy(semaphore_t sem);
 *    Deallocate a semaphore.
 */
void
semaphore_destroy(semaphore_t sem) {
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
semaphore_initialize(semaphore_t sem, int cnt) {
    queue_t q;
    if (NULL == sem)
        return;
    q = queue_new();
    if (NULL == q)
        return;
    sem->count = cnt;
    sem->lock = 0;
    sem->wait = q;
}

/*
 * semaphore_P(semaphore_t sem)
 *    P on the sempahore.
 */
void
semaphore_P(semaphore_t sem) {
    interrupt_level_t oldlevel;
    while (1 == atomic_test_and_set(&(sem->lock)))
        ;
    if (0 > --(sem->count)) {
        oldlevel = set_interrupt_level(DISABLED);
        queue_append(sem->wait, (void*) minithread_self());
        minithread_unlock_and_stop(&(sem->lock));
        set_interrupt_level(oldlevel);
    } else {
        atomic_clear(&(sem->lock));
    }
}

/*
 * semaphore_V(semaphore_t sem)
 *    V on the sempahore.
 */
void
semaphore_V(semaphore_t sem) {
    minithread_t t;
    interrupt_level_t oldlevel;
    while (1 == atomic_test_and_set(&(sem->lock)))
        ;
    if (0 >= ++(sem->count)) {
        oldlevel = set_interrupt_level(DISABLED);
        queue_dequeue(sem->wait,(void**)&t);
        minithread_start(t);
        set_interrupt_level(oldlevel);
    }
    atomic_clear(&(sem->lock));
}
