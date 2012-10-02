#include <stdlib.h>
#include <stdio.h>
#include "interrupts.h"
#include "alarm.h"
#include "alarm_queue.h"
#include "alarm_private.h"

/* Nearest alarm ticks to fire */
long alarmtime;

/* Next alarm id */
int next_alarm_id = 0;

/* Alarm queue */
alarm_queue_t alarmclock;

/*
 * insert alarm event into the alarm queue
 * returns an "alarm id", which is an integer that identifies the
 * alarm.
 */
int
register_alarm(int delay, void (*func)(void*), void *arg)
{
    alarm_t alarm = alarm_create(delay, func, arg);
    interrupt_level_t oldlevel;
    if (alarm == NULL) {
        return -1;
    }
    /* Update nearest alarm to fire */
    oldlevel = set_interrupt_level(DISABLED);
    if (alarm->time_to_fire < alarmtime || -1 == alarmtime) {
        alarmtime = alarm->time_to_fire;
    }
    alarm_queue_insert(alarmclock, alarm);
    set_interrupt_level(oldlevel);
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
    interrupt_level_t oldlevel = set_interrupt_level(DISABLED);
    alarm_queue_delete_by_id(alarmclock, alarmid, &alarm);
    set_interrupt_level(oldlevel);
}

/*
 * Create a new alarm structure
 * Return NULL on failure, pointer to the alarm on success
 */
alarm_t
alarm_create(int delay, void (*func)(void*), void *arg)
{
    alarm_t alarm = malloc(sizeof(struct alarm));
    if (alarm == NULL) {
        fprintf(stderr, "Can't create alarm!\n");
        return NULL;
    }
    alarm->alarm_id = next_alarm_id++;
    alarm->time_to_fire = ticks + (delay * MILLISEC / PERIOD);
    if (alarm->time_to_fire == ticks)
        alarm->time_to_fire++; /* Avoid setting alarm to the current tick */
    alarm->func = func;
    alarm->arg = arg;
    alarm->next = NULL;
    alarm->prev = NULL;
    return alarm;
}

/* Fire all alarms before or at the current tick */
void
alarm_signal()
{
    alarm_t alarm = NULL;
    while ((alarmtime = get_latest_time(alarmclock)) != -1
            && alarmtime <= ticks) {
        if (-1 != alarm_queue_dequeue(alarmclock, &alarm)) {
            alarm->func(alarm->arg);
            free(alarm);
        }
    }
}

/* Initialize alarm structure */
int
alarm_initialize()
{
    if (NULL == (alarmclock = alarm_queue_new()))
        return -1;
    alarmtime = -1;
    return 0;
}

