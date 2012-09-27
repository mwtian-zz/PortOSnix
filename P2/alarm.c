#include <stdio.h>

#include "interrupts.h"
#include "alarm.h"
#include "queue.h"
#include "alarm_private.h"
#include "synch.h"
#include "minithread_private.h"
#include "interrupts.h"
#include "minithread.h"
#include "alarm_queue.h"

/* Next alarm id */
int next_alarm_id = 0;
/* Nearest alarm ticks to fire */
long wakeup;

/*
 * insert alarm event into the alarm queue
 * returns an "alarm id", which is an integer that identifies the
 * alarm.
 */
int
register_alarm(int delay, void (*func)(void*), void *arg)
{
	alarm_t alarm;
	alarm = create_alarm(delay, func, arg);
	if (alarm == NULL) {
		return -1;
	}
	
	/* Should disable interrupt or use semaphore here */
	/* TO BE DONE */
	alarm_queue_insert(alarm_queue, (void*) alarm);
	
	return alarm->alarm_id;
}

/*
 * delete a given alarm
 * it is ok to try to delete an alarm that has already executed.
 */
void
deregister_alarm(int alarmid)
{
	alarm_t alarm;
	alarm_queue_delete_by_id(alarm_queue, alarmid, (void**) &alarm);
}

/* Create an alarm */
alarm_t
create_alarm(int delay, void (*func)(void*), void *arg) {
	alarm_t alarm = (alarm_t) malloc(sizeof(struct alarm));
	if (alarm == NULL) {
		fprintf(stderr, "Can't create alarm!\n");
		return NULL;
	}

	semaphore_P(alarm_id_sem);
	alarm->alarm_id = next_alarm_id++;
	semaphore_V(alarm_id_sem);

	printf("Ticks is %ld\n", ticks);
	alarm->func = func;
	alarm->time_to_fire = ticks + (delay * MILLISEC / PERIOD);
	alarm->arg = arg;
	alarm->next = NULL;
	alarm->prev = NULL;

	/* Update nearest alarm to fire */
	semaphore_P(wakeup_sem);
	if (alarm->time_to_fire < wakeup) {
		wakeup = alarm->time_to_fire;
	}
	semaphore_V(wakeup_sem);

	return alarm;
}
