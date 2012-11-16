/*
 *	Implementation of minimsgs and miniports.
 */
#include "defs.h"
#include "interrupts.h"

#include "miniroute.h"
#include "miniheader.h"
#include "minimsg.h"
#include "minimsg_private.h"
#include "network.h"
#include "queue.h"
#include "queue_wrap.h"
#include "synch.h"

/* Array of pointer to ports, statically initialized to NULL */
static miniport_t port[MAX_BOUNDED - MIN_UNBOUNDED + 1];
/* Number of bounded ports */
static int bound_count = 0;
/* Has the bounded port number wrapped around yet or not */
static char bound_wrap = 0;
/*
 * Makes port creation and destroy minithread safe.
 * Avoiding destroying ports during send/receive of another thread
 * is the responsibility of the user program.
 */
static semaphore_t port_mutex = NULL;

/* File scope helper functions */
static int
miniport_get_boundedport_num();
static void
minimsg_packhdr(mini_header_t hdr, miniport_t unbound, miniport_t bound);

/* Performs any required initialization of the minimsg layer. */
void
minimsg_initialize()
{
    if ((port_mutex = semaphore_create()) != NULL)
        semaphore_initialize(port_mutex, 1);
}

/* Creates an unbound port for listening. Multiple requests to create the same
 * unbound port should return the same miniport reference. It is the responsibility
 * of the programmer to make sure he does not destroy unbound miniports while they
 * are still in use by other threads -- this would result in undefined behavior.
 * Unbound ports must range from 0 to 32767. If the programmer specifies a port number
 * outside this range, it is considered an error.
 */
miniport_t
miniport_create_unbound(int port_number)
{
    semaphore_P(port_mutex);
    if (port_number < MIN_UNBOUNDED || port_number > MAX_UNBOUNDED) {
        semaphore_V(port_mutex);
        return NULL;
    }
    if (port[port_number] != NULL) {
        semaphore_V(port_mutex);
        return port[port_number];
    }
    if ((port[port_number] = malloc(sizeof(struct miniport))) != NULL) {
        port[port_number]->type = UNBOUNDED;
        port[port_number]->num = port_number;
        port[port_number]->unbound.data = queue_new();
        port[port_number]->unbound.ready = semaphore_create();
        if (NULL == port[port_number]->unbound.data
                || NULL == port[port_number]->unbound.ready) {
            miniport_destroy(port[port_number]);
            return NULL;
        }
        semaphore_initialize(port[port_number]->unbound.ready, 0);
    }
    semaphore_V(port_mutex);
    return port[port_number];
}

/* Creates a bound port for use in sending packets. The two parameters, addr and
 * remote_unbound_port_number together specify the remote's listening endpoint.
 * This function should assign bound port numbers incrementally between the range
 * 32768 to 65535. Port numbers should not be reused even if they have been destroyed,
 * unless an overflow occurs (ie. going over the 65535 limit) in which case you should
 * wrap around to 32768 again, incrementally assigning port numbers that are not
 * currently in use.
 */
miniport_t
miniport_create_bound(network_address_t addr, int remote_unbound_port_number)
{
    int num;
    semaphore_P(port_mutex);
    num = miniport_get_boundedport_num();
    if (num < MIN_BOUNDED || num > MAX_BOUNDED) {
        semaphore_V(port_mutex);
        return NULL;
    }
    if ((port[num] = malloc(sizeof(struct miniport))) != NULL) {
        port[num]->type = BOUNDED;
        port[num]->num = num;
        network_address_copy(addr, port[num]->bound.addr);
        port[num]->bound.remote = remote_unbound_port_number;
    }
    semaphore_V(port_mutex);
    return port[num];
}

/* Get the next available bounded port number. Return 0 if none available. */
static int
miniport_get_boundedport_num()
{
    int num;
    int i;
    if (bound_count > NUM_PORTTYPE) {
        return 0;
    }
    if (bound_wrap == 0) {
        num = (bound_count++) + MIN_BOUNDED;
        if (bound_count >= NUM_PORTTYPE) {
            bound_wrap = 1;
        }
    } else {
        for (i = MIN_BOUNDED; i <= MAX_BOUNDED; ++i) {
            if (NULL == port[i]) {
                num = i;
                ++bound_count;
                break;
            }
        }
    }
    return num;
}

/*
 * Destroys a miniport and frees up its resources. If the miniport was in use at
 * the time it was destroyed, subsequent behavior is undefined.
 */
void
miniport_destroy(miniport_t miniport)
{
    semaphore_P(port_mutex);
    if (NULL == miniport) {
        semaphore_V(port_mutex);
        return;
    }
    if (UNBOUNDED == miniport->type) {
        queue_free(miniport->unbound.data);
        semaphore_destroy(miniport->unbound.ready);
    }
    if (BOUNDED == miniport->type) {
        --bound_count;
    }
    port[miniport->num] = NULL;
    free(miniport);
    semaphore_V(port_mutex);
}

/*
 * Sends a message through a locally bound port (the bound port already has an associated
 * receiver address so it is sufficient to just supply the bound port number). In order
 * for the remote system to correctly create a bound port for replies back to the sending
 * system, it needs to know the sender's listening port (specified by local_unbound_port).
 * The msg parameter is a pointer to a data payload that the user wishes to send and does not
 * include a network header; your implementation of minimsg_send must construct the header
 * before calling network_send_pkt(). The return value of this function is the number of
 * data payload bytes sent not inclusive of the header.
 * Return -1 if len is larger then the maximum package size.
 */
int
minimsg_send(miniport_t local_unbound_port, miniport_t local_bound_port,
             minimsg_t msg, int len)
{
    struct mini_header hdr;
    network_address_t dest;
    int sent;

    if (NULL == local_unbound_port || NULL == local_bound_port
            || UNBOUNDED != local_unbound_port->type
            || BOUNDED != local_bound_port->type
            || NULL == msg || len < 0 || len > MINIMSG_MAX_MSG_SIZE)
        return -1;

    minimsg_packhdr(&hdr, local_unbound_port, local_bound_port);
    network_address_copy(local_bound_port->bound.addr, dest);
    sent = miniroute_send_pkt(dest, MINIMSG_HDRSIZE, (char*)&hdr, len, msg);

    return sent - MINIMSG_HDRSIZE < 0 ? -1 : sent - MINIMSG_HDRSIZE;
}

/*
 * Pack header using the local receiving and sending ports.
 * Called by minimsg_sent.
 */
static void
minimsg_packhdr(mini_header_t hdr, miniport_t unbound, miniport_t bound)
{
    hdr->protocol = PROTOCOL_MINIDATAGRAM;
    pack_address(hdr->source_address, hostaddr);
    pack_unsigned_short(hdr->source_port, unbound->num);
    pack_address(hdr->destination_address, bound->bound.addr);
    pack_unsigned_short(hdr->destination_port, bound->bound.remote);
}

/* Receives a message through a locally unbound port. Threads that call this function are
 * blocked until a message arrives. Upon arrival of each message, the function must create
 * a new bound port that targets the sender's address and listening port, so that use of
 * this created bound port results in replying directly back to the sender. It is the
 * responsibility of this function to strip off and parse the header before returning the
 * data payload and data length via the respective msg and len parameter. The return value
 * of this function is the number of data payload bytes received not inclusive of the header.
 */
int
minimsg_receive(miniport_t local_unbound_port, miniport_t* new_local_bound_port,
                minimsg_t msg, int *len)
{
    int received;
    miniport_t port;
    network_interrupt_arg_t *intrpt;
    network_address_t dest_addr;
    mini_header_t header;
    unsigned short dest_port;
    interrupt_level_t oldlevel;

    if (NULL == local_unbound_port || NULL == new_local_bound_port
            || UNBOUNDED != local_unbound_port->type
            || NULL == len || BOUNDED == local_unbound_port->type)
        return -1;

    /*
     * These shared data structures can be changed in the newtwork interrupt
     * handler, so interrupts should be disabled here as well.
     */
    oldlevel = set_interrupt_level(DISABLED);
    semaphore_P(local_unbound_port->unbound.ready);
    queue_wrap_dequeue(local_unbound_port->unbound.data, (void**) &intrpt);
    set_interrupt_level(oldlevel);

    /*
     * The copied size should be the minimum among the user provided buffer
     * size (original *len), the received data size (received), and
     * MINIMSG_MAX_MSG_SIZE.
     */
    received = intrpt->size - MINIMSG_HDRSIZE;
    if (*len >= received)
        *len = received;
    if (*len >= MINIMSG_MAX_MSG_SIZE)
        *len = MINIMSG_MAX_MSG_SIZE;

    header = (mini_header_t) intrpt->buffer;
    unpack_address(header->source_address, dest_addr);
    dest_port = unpack_unsigned_short(header->source_port);

    port = miniport_create_bound(dest_addr, dest_port);
    if (NULL == port)
        return -1;
    *new_local_bound_port = port;

    memcpy(msg, header + 1, *len);
    free(intrpt);

    return received;
}

/* Place the interrupt structure to the correct unbounded port, or discard it */
int
minimsg_process(network_interrupt_arg_t *intrpt)
{
    interrupt_level_t oldlevel;
    mini_header_t header = (mini_header_t) intrpt->buffer;
    int port_num = unpack_unsigned_short(header->destination_port);
    if (port_num < MIN_UNBOUNDED || port_num > MAX_UNBOUNDED
            || NULL == port[port_num]) {
        free(intrpt);
        return -1;
    }

    oldlevel = set_interrupt_level(DISABLED);
    if (queue_wrap_enqueue(port[port_num]->unbound.data, intrpt) != 0) {
        free(intrpt);
        set_interrupt_level(oldlevel);
        return -1;
    }
    semaphore_V(port[port_num]->unbound.ready);
    set_interrupt_level(oldlevel);

    return 0;
}
