#ifndef __NETWORK_MSG_H__
#define __NETWORK_MSG_H__

#include "queue_private.h"

struct msg_node {
    struct node node;
    network_interrupt_arg_t *intrpt;
};

struct msg_tosend {
    int retry;
};


#endif /* __NETWORK_MSG_H__ */
