#ifndef __MINIMSG_PRIVATE_H__
#define __MINIMSG_PRIVATE_H__

#include "queue.h"
#include "queue_private.h"
#include "synch.h"

/* Constatns for miniports only */

#define MIN_UNBOUNDED 0
#define MAX_UNBOUNDED 32767
#define MIN_BOUNDED 32768
#define MAX_BOUNDED 65535
#define NUM_PORTTYPE 32768

/* Struct definitions for miniports and messages */

struct miniport {
    enum port_type {
        UNBOUNDED,
        BOUNDED
    } type;
    int num;
    union {
        struct {
            queue_t data;
            semaphore_t ready;
        } unbound;
        struct {
            network_address_t addr;
            int remote;
        } bound;
    };
};

struct msg_node {
    struct node node;
    network_interrupt_arg_t *intrpt;
};

#endif /*__MINIMSG_PRIVATE_H__*/
