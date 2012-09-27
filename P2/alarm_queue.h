/* alarm_queue.h: prototype for alarm queue operation */
#ifndef __ALARM_QUEUE_H__
#define __ALARM_QUEUE_H__

#include "alarm_private.h"

typedef struct alarm_queue* alarm_queue_t;

/* 
 * Create a new alarm_queue 
 * Return NULL on failure, pointer to queue on success
 */
extern alarm_queue_t alarm_queue_new();

/*
 * Insert an alarm into alarm queue
 * The queue should be sorted by fire time
 * Return 0 on success, -1 on failure
 */
extern int alarm_queue_insert(alarm_queue_t, void*);

/*
 * Dequeue an alarm from alarm queue
 * Return 0 on success, -1 on failure
 */
extern int alarm_queue_dequeue(alarm_queue_t, void**);

/*
 * Delete an alarm from alarm queue
 * Return 0 on success, -1 on failure
 */
extern int alarm_queue_delete(alarm_queue_t, void**);

/*
 * Delete an alarm from alarm queue based on its alarm id
 * Return 0 on success, -1 on failure
 */
extern int alarm_queue_delete_by_id(alarm_queue_t, int, void**);

/*
 * Return the queue length
 * Return -1 if the queue is NULL
 */
extern int alarm_queue_length(alarm_queue_t);

/*
 * Free an alarm queue
 */
extern int alarm_queue_free(alarm_queue_t);

/*
 * Print queue content for debugging
 */
 extern void alarm_queue_print(alarm_queue_t);
 
#endif /* __ALARM_QUEUE_H__ */