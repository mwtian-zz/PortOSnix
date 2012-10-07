#ifndef __ALARM_H_
#define __ALARM_H_

/*
 * This is the alarm interface. You should implement the functions for these
 * prototypes, though you may have to modify some other files to do so.
 */

/* register an alarm to go off in "delay" milliseconds, call func(arg) */
int register_alarm(int delay, void (*func)(void*), void *arg);

void deregister_alarm(int alarmid);

#endif
