#include <stdio.h>
#include "alarm.h"
#include "alarm_queue.h"
#include "alarm_private.h"
void main() {
	alarm_t a1, a2, a3, a4, a5, a6;
	alarm_queue_t alarm_queue;

	a1 = create_alarm(100, NULL, NULL);
	a2 = create_alarm(500, NULL, NULL);
	a3 = create_alarm(2000, NULL, NULL);
	a4 = create_alarm(1000, NULL, NULL);
	a5 = create_alarm(400, NULL, NULL);

	alarm_queue = alarm_queue_new();
	alarm_queue_insert(alarm_queue, a1);
	alarm_queue_print(alarm_queue);
	alarm_queue_insert(alarm_queue, a2);
	alarm_queue_insert(alarm_queue, a3);
	alarm_queue_print(alarm_queue);
	alarm_queue_delete_by_id(alarm_queue, 0, &a6);
	printf("%ld\n", a6->time_to_fire);
	alarm_queue_delete_by_id(alarm_queue, 1, &a6);
	printf("%ld\n", a6->time_to_fire);
	alarm_queue_delete(alarm_queue, &a2);
	alarm_queue_print(alarm_queue);
	alarm_queue_dequeue(alarm_queue, &a6);
	printf("%ld", a6->time_to_fire);
	alarm_queue_delete(alarm_queue, &a3);
	printf("%ld", a3->time_to_fire);
	alarm_queue_delete(alarm_queue, &a2);

	alarm_queue_free(alarm_queue);
}
