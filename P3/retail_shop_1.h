/* 
 * This file contains the structure of a phone
 * and prototype of employee and customer thread
 * functions 
 */
 
#ifndef __RETAIL_SHOP_H__
#define __RETAIL_SHOP_H__

#include "queue.h"

/* Phone structure */
struct phone {
	struct phone* prev;
	struct phone* next;
	struct queue* queue;
	int serial_num;
};

typedef struct phone* phone_t;

/* Unpack a new phone */
phone_t phone_create();

#endif /* __RETAIL_SHOP_H__ */