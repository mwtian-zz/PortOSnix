#include <stdlib.h>

#include "queue.h"
#include "queue_wrap.h"
#include "queue_locked.h"

struct queue_locked {
    node_t head;
    node_t tail;
    int length;
    semaphore_t lock;
};

queue_locked_t
queue_locked_new()
{
    queue_locked_t q;
    if ((q = malloc(sizeof(struct queue_locked))) ==  NULL)
        return NULL;
    if ((q->lock = semaphore_create()) == NULL) {
        free(q);
        return NULL;
    }
    semaphore_initialize(q->lock, 1);

    return q;
}

int
queue_locked_prepend(queue_locked_t q, void* data)
{
    int temp;
    if (NULL == q)
        return -1;
    semaphore_P(q->lock);
    temp = queue_locked_prepend(q, data);
    semaphore_V(q->lock);

    return temp;
}

int
queue_locked_enqueue(queue_locked_t q, void* data)
{
    int temp;
    if (NULL == q)
        return -1;
    semaphore_P(q->lock);
    temp = queue_locked_enqueue(q, data);
    semaphore_V(q->lock);

    return temp;
}

int
queue_locked_dequeue(queue_locked_t q, void** data)
{
    int temp;
    if (NULL == q)
        return -1;
    semaphore_P(q->lock);
    temp = queue_locked_dequeue(q, data);
    semaphore_V(q->lock);

    return temp;
}

void
queue_locked_free(queue_locked_t q)
{
    if (q != NULL)
        semaphore_destroy(q->lock);
    free(q);
}
