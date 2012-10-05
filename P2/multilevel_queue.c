/*
 * Multilevel queue manipulation functions
 */
#include <stdlib.h>
#include <stdio.h>
#include "queue.h"
#include "multilevel_queue.h"
#include "multilevel_queue_private.h"

/*
 * Returns an empty multilevel queue with number_of_levels levels. On error should return NULL.
 */
multilevel_queue_t
multilevel_queue_new(int number_of_levels)
{
    int i;
    multilevel_queue_t mq = malloc(sizeof(struct multilevel_queue));
    if (NULL == mq)
        return NULL;
    mq->lvl = number_of_levels;
    mq->q = malloc(mq->lvl * sizeof(struct multilevel_queue));
    if (NULL == mq->q) {
        multilevel_queue_free(mq);
        return NULL;
    }
    for (i = 0; i < mq->lvl; ++i)
        mq->q[i] = NULL;
    for (i = 0; i < mq->lvl; ++i) {
        mq->q[i] = queue_new();
        if (NULL == mq->q[i]) {
            multilevel_queue_free(mq);
            return NULL;
        }
    }
    return mq;
}

/*
 * Appends an void* to the multilevel queue at the specified level. Return 0 (success) or -1 (failure).
 */
int
multilevel_queue_enqueue(multilevel_queue_t queue, int level, void* item)
{
    if (NULL == queue || level < 0 || level >= queue->lvl)
        return -1;
    return queue_append(queue->q[level], item);
}

/*
 * Dequeue and return the first void* from the multilevel queue starting at the specified level.
 * Levels wrap around so as long as there is something in the multilevel queue an item should be returned.
 * Return the level that the item was located on and that item if the multilevel queue is nonempty,
 * or -1 (failure) and NULL if queue is empty.
 */
int
multilevel_queue_dequeue(multilevel_queue_t queue, int level, void** item)
{
    int i;
    if (NULL == queue || level < 0 || level >= queue->lvl)
        return -1;
    for (i = 0; i < queue->lvl; ++i) {
        if (0 == queue_dequeue(queue->q[(level + i) % queue->lvl], item))
            return i;
    }
    *item = NULL;
    return -1;
}

/*
 * Free the queue and return 0 (success) or -1 (failure). Do not free the queue nodes; this is
 * the responsibility of the programmer.
 */
int
multilevel_queue_free(multilevel_queue_t queue)
{
    int i;
    if (NULL == queue)
        return -1;
    for (i = 0; i < queue->lvl; ++i)
        queue_free(queue->q[i]);
    return 0;
}
