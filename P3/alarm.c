#include <stdio.h>

#include "interrupts.h"
#include "alarm.h"
#include "minithread.h"
#include "queue.h"

/*
 * insert alarm event into the alarm queue
 * returns an "alarm id", which is an integer that identifies the
 * alarm.
 */
int register_alarm(int delay, void (*func)(void*), void *arg)
{

	return -1;
}

/*
 * delete a given alarm  
 * it is ok to try to delete an alarm that has already executed.
 */
void deregister_alarm(int alarmid)
{

}

