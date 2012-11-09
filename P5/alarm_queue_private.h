/* alarm_queue_private.h: alarm queue structure */
#ifndef __ALARM_QUEUE_PRIVATE_H__
#define __ALARM_QUEUE_PRIVATE_H__

struct alarm_queue {
	alarm_t head;
	alarm_t tail;
	int length;
};

#endif /* __ALARM_QUEUE_PRIVATE_H__ */
