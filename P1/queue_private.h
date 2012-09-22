/*
 * Internal data structure for queue
 */

#ifndef __QUEUE_PRIVATE_H__
#define __QUEUE_PRIVATE_H__

#include "queue.h"

/*
 * The node data structure.
 * Any other structures which need to be manipulated by queue-related
 * functions should have first three fields correspond to node.
 */
struct node {
    struct node *prev;
    struct node *next;
    struct queue *queue;
};

/*
 * node_t is a pointer to a struct that has its first three elements
 * correspond to node. node_t is used to manipulate the TCB.
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
