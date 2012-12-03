#include <stdio.h>
#include "alarm.h"
#include "alarm_queue.h"
#include "alarm_private.h"

int
main()
{
    alarm_t a1, a2, a3, a6;
    alarm_queue_t alarm_queue;

    a1 = alarm_create(100, NULL, NULL);
    a2 = alarm_create(500, NULL, NULL);
    a3 = alarm_create(2000, NULL, NULL);

    alarm_queue = alarm_queue_new();
    alarm_queue_insert(alarm_queue, a1);
    alarm_queue_insert(alarm_queue, a2);
    alarm_queue_insert(alarm_queue, a3);
    printf("id of a1: %d\n", a1->alarm_id);
    printf("id of a2: %d\n", a2->alarm_id);
    printf("id of a3: %d\n", a3->alarm_id);
    alarm_queue_delete_by_id(alarm_queue, 0, &a6);
    printf("Delete alarm id 0, ticks to fire: %ld\n", a6->time_to_fire);
    alarm_queue_delete_by_id(alarm_queue, 1, &a6);
    printf("Delete alarm id 1, ticks to fire: %ld\n", a6->time_to_fire);
    alarm_queue_dequeue(alarm_queue, &a6);
    printf("Dequeue, ticks to fire: %ld\n", a6->time_to_fire);
    free(a1);
    free(a2);
    free(a3);
    alarm_queue_free(alarm_queue);
    return 0;
}
