#include <stdio.h>

#include "interrupts.h"
#include "alarm.h"
#include "minithread.h"
#include "queue.h"
#include "alarm_private.h"
#include "synch.h"
#include "minithread_private.h"

/* Next alarm id */
int next_alarm_id = 0;

/* Current ticks */
extern int long ticks;
extern struct thread_monitor thread_monitor;

/* Alarm id semaphore */
extern semaphore_t alarm_id_sem;
/* Thread monitor semaphore */
extern semaphore_t thread_monitor_sem;

/*
 * insert alarm event into the alarm queue
 * returns an "alarm id", which is an integer that identifies the
 * alarm.
 */
int 
register_alarm(int delay, void (*func)(void*), void *arg)
{

	return -1;
}

/*
 * delete a given alarm  
 * it is ok to try to delete an alarm that has already executed.
 */
void 
deregister_alarm(int alarmid)
{

}

/* Create an alarm */
alarm_t
create_alarm(int delay, void (*func)(void*), void *arg) {
	alarm_t alarm = (alarm_t) malloc(sizeof(struct alarm));
	if (alarm == NULL) {
		fprintf(stderr, "Can't create alarm!\n");
		return NULL;
	}
	
	/*semaphore_P(alarm_id_sem);*/
	alarm->alarm_id = next_alarm_id++;
	/*semaphore_V(alarm_id_sem);*/
	
	printf("Ticks is %ld\n", ticks);
	alarm->func = func;
	alarm->time_to_fire = ticks + (delay * MILLISEC / PERIOD);
	alarm->arg = arg;
	alarm->next = NULL;
	alarm->prev = NULL;
	
	/* Update nearest alarm to fire */
	/*semaphore_P(thread_monitor_sem);*/
	if (alarm->time_to_fire < thread_monitor.alarm) {
		thread_monitor.alarm = alarm->time_to_fire;
	}
	/*semaphore_V(thread_monitor_sem);*/
	
	return alarm;
}