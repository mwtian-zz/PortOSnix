/* Function implementation in alarm_queue.h */

#include <stdio.h>
#include <stdlib.h>

#include "alarm.h"
#include "alarm_queue.h"
#include "alarm_private.h"

/*
 * Create a new alarm_queue
 * Return NULL on failure, pointer to queue on success
 */
alarm_queue_t
alarm_queue_new()
{
    alarm_queue_t new_queue = (alarm_queue_t) malloc(sizeof(struct alarm_queue));
    if (new_queue == NULL) {
        fprintf(stderr, "Can't create alarm queue!");
        return NULL;
    }

    new_queue->head = NULL;
    new_queue->tail = NULL;
    new_queue->length = 0;

    return new_queue;
}

/*
 * Insert an alarm into alarm queue
 * The queue should be sorted by fire time
 * Return 0 on success, -1 on failure
 */
int
alarm_queue_insert(alarm_queue_t alarm_queue, alarm_t alarm)
{
    alarm_t cur;
    if (alarm_queue == NULL) {
        return -1;
    }

    /* Empty queue */
    if (alarm_queue->head == NULL && alarm_queue->tail == NULL) {
        alarm_queue->head = alarm;
        alarm_queue->tail = alarm;
        alarm_queue->length += 1;
        return 0;
    }

    /* Find the insertion point */
    for (cur = alarm_queue->head; cur != NULL
            && cur->time_to_fire < alarm->time_to_fire; cur = cur->next);

    /* Insert before head */
    if (cur == alarm_queue->head) {
        alarm->prev = NULL;
        alarm->next = cur;
        alarm_queue->head = alarm;
        alarm_queue->length += 1;
        return 0;
    }

    /* Insert after tail */
    if (cur == NULL) {
        alarm_queue->tail->next = alarm;
        alarm->prev = alarm_queue->tail;
        alarm->next = NULL;
        alarm_queue->tail = alarm;
        alarm_queue->length += 1;
        return 0;
    }

    /* Insert in middle */
    cur->prev->next = alarm;
    alarm->prev = cur->prev;
    alarm->next = cur;
    cur->prev = alarm;
    alarm_queue->length += 1;
    return 0;
}

/*
 * Dequeue an alarm from alarm queue
 * Return 0 on success, -1 on failure
 */
int
alarm_queue_dequeue(alarm_queue_t alarm_queue, alarm_t *data)
{
    if (data == NULL) {
        return -1;
    }
    if (alarm_queue == NULL) {
        *data = NULL;
        return -1;
    }

    *data = alarm_queue->head;
    if (*data == NULL) {
        return -1;
    }

    return alarm_queue_delete(alarm_queue, data);
}

/*
 * Delete an alarm from alarm queue
 * Return 0 on success, -1 on failure
 */
int
alarm_queue_delete(alarm_queue_t alarm_queue, alarm_t *data)
{
    alarm_t alarm;

    if (alarm_queue == NULL || data == NULL || *data == NULL) {
        return -1;
    }

    alarm = *data;
    /* Head */
    if (alarm == alarm_queue->head) {
        alarm_queue->head = alarm->next;
    }
    /* Tail */
    if (alarm == alarm_queue->tail) {
        alarm_queue->tail = alarm->prev;
    }

    if (alarm->prev) {
        alarm->prev->next = alarm->next;
    }
    if (alarm->next) {
        alarm->next->prev = alarm->prev;
    }

    alarm->prev = NULL;
    alarm->next = NULL;
    alarm_queue->length--;

    return 0;
}

/*
 * Delete an alarm from alarm queue based on its alarm id
 * Return 0 on success, -1 on failure
 */
int
alarm_queue_delete_by_id(alarm_queue_t alarm_queue, int alarm_id, alarm_t *data)
{
    alarm_t alarm;
    int ret;

    if (alarm_queue == NULL || data == NULL) {
        return -1;
    }

    /* Find alarm with alarm_id */
    for (alarm = alarm_queue->head; alarm != NULL && alarm->alarm_id != alarm_id; alarm = alarm->next);
    /* Delete it */
    ret = alarm_queue_delete(alarm_queue, &alarm);
    *data = alarm;

    return ret;
}

/*
 * Return lastest wake up time
 * Return -1 if queue is empty
 */
long
get_latest_time(alarm_queue_t alarm_queue)
{
    if (alarm_queue->head != NULL) {
        return alarm_queue->head->time_to_fire;
    }
    return -1;
}


/*
 * Return the queue length
 * Return -1 if the queue is NULL
 */
int
alarm_queue_length(alarm_queue_t alarm_queue)
{
    if (alarm_queue == NULL) {
        return -1;
    }
    return alarm_queue->length;
}

/*
 * Free an alarm queue
 */
int
alarm_queue_free(alarm_queue_t alarm_queue)
{
    if (alarm_queue == NULL) {
        return -1;
    }
    free(alarm_queue);
    return 0;
}

/*
 * Print alarm queue content for debugging
 */
void
alarm_queue_print(alarm_queue_t alarm_queue)
{
    alarm_t head;

    printf("Alarm queue starts\n");
    for (head = alarm_queue->head; head != NULL; head = head->next) {
        printf("Alarm id is %d, fire time is %ld\n", head->alarm_id, head->time_to_fire);
    }
    printf("Alarm queue ends\n");
}
