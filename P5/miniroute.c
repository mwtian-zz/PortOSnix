#include "defs.h"

#include "queue.h"
#include "queue_wrap.h"
#include "synch.h"
#include "minithread.h"
#include "miniroute.h"
#include "miniheader.h"
#include "minimsg.h"
#include "minisocket.h"

static void
miniroute_relay(network_interrupt_arg_t *intrpt);
static int
miniroute_pack_data_hdr(miniroute_header_t hdr, network_address_t dest_address,
                        network_address_t next_hop_addr);
static int
miniroute_control(int *arg);
static void
miniroute_wait_for_intrpt(network_interrupt_arg_t **p_intrpt);
static void
miniroute_process_data(network_interrupt_arg_t *intrpt);
static void
miniroute_process_discovery(network_interrupt_arg_t *intrpt);
static void
miniroute_process_reply(network_interrupt_arg_t *intrpt);

/* Store network interrupts. Protected by disabling interrupts. */
static queue_t intrpt_buffer;
/* Signals when network interrupts need to be processed */
static semaphore_t intrpt_sig;

/* Performs any initialization of the miniroute layer, if required. */
void
miniroute_initialize()
{
    intrpt_buffer = queue_new();
    intrpt_sig = semaphore_create();
    if (NULL == intrpt_buffer || NULL == intrpt_sig)
        return;

    semaphore_initialize(intrpt_sig, 0);

    minithread_fork(miniroute_control, NULL);
}

/* sends a miniroute packet, automatically discovering the path if necessary.
 * See description in the .h file.
 */
int
miniroute_send_pkt(network_address_t dest_address, int hdr_len, char* hdr,
                   int data_len, char* data)
{
    int total_len = hdr_len + data_len;
    int sent_len = 0;
    network_address_t next_hop_addr;
    struct routing_header routing_hdr;
    char *total_data = malloc(total_len);
    if (NULL == total_data)
        return -1;

    memcpy(total_data, hdr, hdr_len);
    memcpy(total_data + hdr_len, data, data_len);

    if (miniroute_pack_data_hdr(&routing_hdr, dest_address, next_hop_addr) == 0) {
        sent_len = network_send_pkt(next_hop_addr, MINIROUTE_HDRSIZE,
                                    (char*)&routing_hdr, total_len, total_data);
    }
#if MINIROUTE_DEBUG == 1
    printf("Network packet sent.\n");
#endif
    if (sent_len < hdr_len)
        return -1;
    else
        return sent_len - hdr_len;
}

static int
miniroute_pack_data_hdr(miniroute_header_t hdr, network_address_t dest_address,
                        network_address_t next_hop_addr)
{
    hdr->routing_packet_type = ROUTING_DATA;
    pack_address(hdr->destination, dest_address);
    network_address_copy(dest_address, next_hop_addr);
    return 0;
}

static void
miniroute_relay(network_interrupt_arg_t *intrpt)
{

}

/* hashes a network_address_t into a 16 bit unsigned int */
unsigned short
hash_address(network_address_t address)
{
    unsigned int result = 0;
    int counter;

    for (counter = 0; counter < 3; counter++)
        result ^= ((unsigned short*)address)[counter];

    return result % 65521;
}


int
miniroute_buffer_intrpt(network_interrupt_arg_t *intrpt)
{
#if MINIROUTE_DEBUG == 1
    printf("Network packet received.\n");
#endif
    if (queue_wrap_enqueue(intrpt_buffer, intrpt) == 0) {
        semaphore_V(intrpt_sig);
        return 0;
    }

    return -1;
}

/* Control thread sorting received miniroute packets */
static int
miniroute_control(int *arg)
{
    network_interrupt_arg_t *intrpt;
    miniroute_header_t header;
    while (1) {
        miniroute_wait_for_intrpt(&intrpt);
        if (intrpt->size >= MINIROUTE_HDRSIZE) {
#if MINIROUTE_DEBUG == 1
            printf("Sorting Network packet.\n");
#endif
            header = (miniroute_header_t) intrpt->buffer;
            switch (header->routing_packet_type) {
            case ROUTING_DATA:
                miniroute_process_data(intrpt);
                break;
            case ROUTING_ROUTE_DISCOVERY:
                miniroute_process_discovery(intrpt);
                break;
            case ROUTING_ROUTE_REPLY:
                miniroute_process_reply(intrpt);
                break;
            default:
                free(intrpt);
            }
        } else {
            free(intrpt);
        }
    }
    return 0;
}

/* Wait for available interrupt */
static void
miniroute_wait_for_intrpt(network_interrupt_arg_t **p_intrpt)
{
    interrupt_level_t oldlevel = set_interrupt_level(DISABLED);
    semaphore_P(intrpt_sig);
    queue_wrap_dequeue(intrpt_buffer, (void**)p_intrpt);
    set_interrupt_level(oldlevel);
}

static void
miniroute_process_data(network_interrupt_arg_t *intrpt)
{
    int i;
    interrupt_level_t oldlevel;
    miniroute_header_t routing_hdr = (miniroute_header_t) intrpt->buffer;
    mini_header_t transport_hdr = (mini_header_t) (routing_hdr + 1);
    network_address_t destaddr;
    unpack_address(routing_hdr->destination, destaddr);
#if MINIROUTE_DEBUG == 1
    printf("Processing data in Network packet.\n");
#endif
    /* Relay packets if it is intended for others */
    if (network_address_same(destaddr, hostaddr) != 1)
        miniroute_relay(intrpt);

    /* Drop packets with incomplete header */
    if (intrpt->size < MINIROUTE_HDRSIZE + MINIMSG_HDRSIZE) {
        free(intrpt);
        return;
    } else {
        /* Strip header */
        intrpt->size -= MINIROUTE_HDRSIZE;
        for (i = 0; i < intrpt->size; ++i)
            intrpt->buffer[i] = intrpt->buffer[MINIROUTE_HDRSIZE + i];
    }

    oldlevel = set_interrupt_level(DISABLED);
    switch (transport_hdr->protocol) {
    case PROTOCOL_MINIDATAGRAM:
        minimsg_process(intrpt);
        break;
    case PROTOCOL_MINISTREAM:
        if (intrpt->size >= MINIROUTE_HDRSIZE + MINISOCKET_HDRSIZE)
            minimsg_process(intrpt);
        break;
    default:
        free(intrpt);
    }
    set_interrupt_level(oldlevel);

    return;
}

static void
miniroute_process_discovery(network_interrupt_arg_t *intrpt)
{

}

static void
miniroute_process_reply(network_interrupt_arg_t *intrpt)
{

}
