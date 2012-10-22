#ifndef __MULTILEVEL_QUEUE_PRIVATE_H__
#define __MULTILEVEL_QUEUE_PRIVATE_H__

#include "queue.h"
#include "multilevel_queue.h"

struct multilevel_queue {
    int lvl;
    queue_t *q;
};

#endif /*__MULTILEVEL_QUEUE_PRIVATE_H__*/
