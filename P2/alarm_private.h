/* alarm_private.h: holding alarm structure and alarm queue structure */
#ifndef __ALARM_PRIVATE_H__
#define __ALARM_PRIVATE_H__
#include "synch.h"
struct alarm {
	struct alarm *prev;
	struct alarm *next;
	int alarm_id;
	long time_to_fire;
	void (*func)(void*);
	void *arg;
};

typedef struct alarm *alarm_t;

struct alarm_queue {
	alarm_t head;
	alarm_t tail;
	int length;
};

extern int next_alarm_id;

semaphore_t alarm_id_sem;

#endif /* __ALARM_PRIVATE_H__ */
