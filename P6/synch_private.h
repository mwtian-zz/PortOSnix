#ifndef __SYNCH_PRIVATE_H__
#define __SYNCH_PRIVATE_H__

struct semaphore {
    int count;
    tas_lock_t lock;
    queue_t wait;
};

#endif /*__SYNCH_PRIVATE_H__*/

