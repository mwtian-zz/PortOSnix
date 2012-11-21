#include "stdlib.h"
#include "minimsg.h"
#include "synch.h"
#include "queue.h"
#include "network.h"

#define MIN_UNBOUNDED 0
#define MAX_UNBOUNDED 32767
#define MIN_BOUNDED 32768
#define MAX_BOUNDED 65535
#define NUM_PORTTYPE 32768

struct miniport {
    enum port_type {
        UNBOUNDED,
        BOUNDED
    } type;
    int num;
    union {
        struct {
            queue_t data;
            semaphore_t mutex;
            semaphore_t ready;
        } unbound;
        struct {
            network_address_t addr;
            int remote;
        } bound;
    };
};

miniport_t port[MAX_BOUNDED + 1];

int
main()
{
    int i;
    network_address_t addr;
    addr[0] = 1;
    addr[1] = 2;
    for (i = MIN_UNBOUNDED; i <= MAX_UNBOUNDED; ++i) {
        port[i] = miniport_create_unbound(i);
        if (port[i]->num != i)
            printf("Failure at unbounded port: %d\n", i);
    }

    for (i = MIN_BOUNDED; i <= MAX_BOUNDED; ++i) {
        port[i] = miniport_create_bound(addr, 12);
    }
    if (NULL != miniport_create_bound(addr, 12))
        printf("Failure at counting bounded ports..\n");
    for (i = MIN_UNBOUNDED; i <= MAX_BOUNDED; ++i) {
        miniport_destroy(port[i]);
    }
    return 0;
}
