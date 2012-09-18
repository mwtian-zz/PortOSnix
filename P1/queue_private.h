/*
 * Internal data structure for queue
 */

#ifndef __QUEUE_PRIVATE_H__
#define __QUEUE_PRIVATE_H__

#include "queue.h"

/*
 * The node data structure.
 * Any other structures which wish to be manipulated by queue-related
 * functions need to have a 'prev' pointer as its first field, and a
 * 'next' pointer as its second field;
 */
struct node {
	struct node *prev;
	struct node *next;
	struct queue *queue;
};

/*
 * node_t is a pointer to a struct that has its first element a next
 * pointer. Pointers to other structs including tcb are cast into node_t
 * to be manipulated in the queue, using "poor man's inheritance".
 */
typedef struct node* node_t;

/*
 * The queue data structure.
 */
typedef struct queue {
	node_t head;
	node_t tail;
	int length;
} queue;

#endif /*__QUEUE_PRIVATE_H__*/
