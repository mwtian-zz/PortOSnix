/*
 * Generic queue implementation.
 *
 */
#include <stdlib.h>
#include <stdio.h>
#include "queue.h"
#include "queue_private.h"

/*
 * Return an empty queue. On error, return NULL.
 */
queue_t
queue_new()
{
    queue_t q;
    if ((q = malloc(sizeof(struct queue))) == NULL) {
        return NULL;
    }
    q->length = 0;
    q->head = NULL;
    q->tail = NULL;
    return q;
}

/*
 * Prepend a void* to a queue (both specifed as parameters).  Return
 * 0 (success) or -1 (failure).
 * 'item' is added to the head of the structure.
 * Prepended item is first to be dequeued.
 */
int
queue_prepend(queue_t queue, void* item)
{
    node_t node = item;
    if (NULL == queue || NULL == item)
        return -1;
    if (queue->length == 0)
        queue->tail = node;
    else
        queue->head->prev = node;
    node->prev = NULL;
    node->next = queue->head;
    node->queue = queue;
    queue->head = node;
    ++(queue->length);
    return 0;
}

/*
 * Append a void* to a queue (both specifed as parameters). Return
 * 0 (success) or -1 (failure).
 * 'item' is added to the tail of the structure.
 * Appended item is last to be dequeued.
 */
int
queue_append(queue_t queue, void* item)
{
    node_t node = item;
    if (NULL == queue || NULL == item)
        return -1;
    if (queue->length == 0)
        queue->head = node;
    else
        queue->tail->next = node;
    node->prev = queue->tail;
    node->next = NULL;
    node->queue = queue;
    queue->tail = node;
    ++(queue->length);
    return 0;
}

/*
 * Dequeue and return the first void* from the queue or NULL if queue
 * is empty.  Return 0 (success) or -1 (failure).
 * 'item' is removed from the head of the structure.
 */
int
queue_dequeue(queue_t queue, void** item)
{
    if (NULL == item)
        return -1;
    if (queue == NULL) {
        *item = NULL;
        return -1;
    }
    *item = (void*) queue->head;
    if (NULL == *item)
        return -1;
    return queue_delete(queue, item);
}

/*
 * Delete the specified item from the given queue.
 * Return -1 on error.
 * '*item' is never modified in this function.
 */
int
queue_delete(queue_t queue, void** item)
{
    node_t node;
    if (NULL == queue || NULL == item)
        return -1;
    node = *item;
    if (NULL == node || 0 >= queue->length || queue != node->queue)
        return -1;

    if (node->prev == NULL)
        queue->head = node->next;
    else {
        node->prev->next = node->next;
    }

    if (node->next == NULL)
        queue->tail = node->prev;
    else {
        node->next->prev = node->prev;
    }

    node->prev = NULL;
    node->next = NULL;
    --(queue->length);
    return 0;
}

/*
 * Iterate the function parameter over each element in the queue.  The
 * additional void* argument is passed to the function as its first
 * argument and the queue element is the second.  Return 0 (success)
 * or -1 (failure).
 */
int
queue_iterate(queue_t queue, PFany f, void* item)
{
    node_t current, next;
    if (NULL == queue || NULL == f)
        return -1;
    for (current = queue->head; current != NULL; current = next) {
        next = current->next;
        if (-1 == f(item, current))
            return -1;
    }
    return 0;
}

/*
 * Return the number of items in the queue.
 */
int
queue_length(queue_t queue)
{
    if (NULL == queue)
        return -1;
    return queue->length;
}

/*
 * Free the queue and return 0 (success) or -1 (failure).
 */
int
queue_free (queue_t queue)
{
    if (NULL == queue)
        return -1;
    free(queue);
    return 0;
}
