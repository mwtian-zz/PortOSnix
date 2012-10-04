/* alarm_private.h: holding alarm structure */
#ifndef __ALARM_PRIVATE_H__
#define __ALARM_PRIVATE_H__

struct alarm {
	struct alarm *prev;
	struct alarm *next;
	int alarm_id;
	long time_to_fire;
	void (*func)(void*);
	void *arg;
};

#endif /* __ALARM_PRIVATE_H__ */
