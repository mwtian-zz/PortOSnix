#ifndef __ALARM_H_
#define __ALARM_H_

#include "alarm_private.h"

/*
 * This is the alarm interface. You should implement the functions for these
 * prototypes, though you may have to modify some other files to do so.
 */
extern int next_alarm_id;
/* Nearest alarm ticks to fire */
extern long wakeup;

/* register an alarm to go off in "delay" milliseconds, call func(arg) */
int register_alarm(int delay, void (*func)(void*), void *arg);

void deregister_alarm(int alarmid);

/* Create an alarm */
extern alarm_t create_alarm(int, void (*func)(void*), void*);

#endif /* __ALARM_H_ */
