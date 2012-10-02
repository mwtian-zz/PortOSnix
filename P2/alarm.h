#ifndef __ALARM_H_
#define __ALARM_H_

/*
 * This is the alarm interface. You should implement the functions for these
 * prototypes, though you may have to modify some other files to do so.
 */
typedef struct alarm* alarm_t;

/* Nearest alarm ticks to fire */
extern long alarmtime;

/* The id of the alarm to be fired next */
extern int next_alarm_id;

/* register an alarm to go off in "delay" milliseconds, call func(arg) */
extern int register_alarm(int delay, void (*func)(void*), void *arg);

extern void deregister_alarm(int alarmid);

/* Create an alarm structure */
extern alarm_t alarm_create(int delay, void (*func)(void*), void *arg);

extern void alarm_signal();

extern int alarm_initialize();

#endif /* __ALARM_H_ */
