#ifndef __MINISOCKETS_PRIVATE_H_
#define __MINISOCKETS_PRIVATE_H_

#include "network.h"
#include "queue.h"
#include "synch.h"
#include "synch.h"

struct minisocket
{
    int local_port_num;
	int remote_port_num;
    int seq;
    int ack;
    int alarm;
    queue_t data;
    semaphore_t socket_mutex;
    semaphore_t receive;
    network_address_t addr;
    enum socket_status {
		LISTEN,
		SYNSENT,
		SYNRECEIVED,
		ESTABLISHED,
		CLOSEWAIT,
		LASTACK,
		CLOSED,
		FINWAIT1,
		FINWAIT2,
		CLOSING,
		TIMEWAIT
    } state;
};

#endif /* __MINISOCKETS_PRIVATE_H_ */
