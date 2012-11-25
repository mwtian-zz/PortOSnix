#ifndef __QUEUE_LOCKED_H__
#define __QUEUE_LOCKED_H__

#include "queue_private.h"
#include "queue_wrap.h"
#include "synch.h"


typedef struct queue_locked *queue_locked_t;

/* Functions are defined similar to their counter parts in queue.h */
extern queue_locked_t
queue_locked_new();

extern int
queue_locked_prepend(queue_locked_t q, void* data);

extern int
queue_locked_enqueue(queue_locked_t q, void* data);

extern int
queue_locked_dequeue(queue_locked_t q, void** data);

extern void
queue_locked_free(queue_locked_t q);

#endif /* __QUEUE_LOCKED_H__ */
