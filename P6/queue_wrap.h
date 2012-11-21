#ifndef __QUEUE_WRAP_H__
#define __QUEUE_WRAP_H__

#include "queue_private.h"

struct queue_wrap_node {
    struct node node;
    void *data;
};

extern int
queue_wrap_prepend(queue_t q, void* data);

extern int
queue_wrap_enqueue(queue_t q, void* data);

extern int
queue_wrap_dequeue(queue_t q, void** data);

#endif /* __QUEUE_WRAP_H__ */
