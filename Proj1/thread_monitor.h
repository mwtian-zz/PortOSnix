/*
 * Information for thread managing.
 */
#ifndef __THREAD_MONITOR_H__
#define __THREAD_MONITOR_H__

#include "queue.h"
#include "minithread.h"

struct thread_monitor {
	int count;
	int tidcount;
	minithread_t instack;
	queue_t ready;
	queue_t blocked;
	queue_t exited;
};

#endif /*__THREAD_MONITOR_H__*/
