#include <stdio.h>

#include "interrupts.h"
#include "queue.h"
#include "synch.h"
#include "minithread.h"
#include "alarm.h"
#include "alarm_queue.h"
#include "alarm_private.h"

/* Nearest alarm ticks to fire */
long wakeup;

/* Next alarm id */
int next_alarm_id = 0;

/* Alarm queue */
alarm_queue_t alarm_queue;

/*
 * insert alarm event into the alarm queue
 * returns an "alarm id", which is an integer that identifies the
 * alarm.
 */
int
register_alarm(int delay, void (*func)(void*), void *arg)
{
    alarm_t alarm = create_alarm(delay, func, arg);
    if (alarm == NULL) {
        return -1;
    }

    /* Update nearest alarm to fire */
    if (alarm->time_to_fire < wakeup || -1 == wakeup) {
        wakeup = alarm->time_to_fire;
    }

    alarm_queue_insert(alarm_queue, alarm);
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
    alarm_queue_delete_by_id(alarm_queue, alarmid, &alarm);
    set_interrupt_level(oldlevel);
}

/*
 * Create a new alarm structure
 * Return NULL on failure, pointer to the alarm on success
 */
alarm_t
create_alarm(int delay, void (*func)(void*), void *arg)
{
    alarm_t alarm = (alarm_t) malloc(sizeof(struct alarm));
    if (alarm == NULL) {
        fprintf(stderr, "Can't create alarm!\n");
        return NULL;
    }
    alarm->alarm_id = next_alarm_id++;
    /* printf("Ticks is %ld\n", ticks); */
    alarm->func = func;
    alarm->time_to_fire = ticks + (delay * MILLISEC / PERIOD);
    alarm->arg = arg;
    alarm->next = NULL;
    alarm->prev = NULL;
    return alarm;
}

/* Check if any alarm needs to be fired */
void
signal_alarm()
{
    alarm_t alarm = NULL;
    long fire_time;
    while (1) {
        fire_time = get_latest_time(alarm_queue);
        if (fire_time == -1) {
            wakeup = -1;
            break;
        } else {
            /* Can be fired */
            if (fire_time <= ticks) {
                alarm_queue_dequeue(alarm_queue, &alarm);
                if (alarm) {
                    /* Fire alarm */
                    alarm->func(alarm->arg);
                    free(alarm);
                }
            } else {
                /* Not yet to fire, update next wakeup time */
                wakeup = fire_time;
                break;
            }
        }
    }
}
