#include "defs.h"

#include "queue.h"
#include "queue_wrap.h"
#include "synch.h"
#include "minithread.h"
#include "miniroute.h"
#include "miniheader.h"
#include "minimsg.h"
#include "minisocket.h"
#include "miniroute_cache.h"
#include "miniroute_cache_private.h"

/* File scope functions. Explained before each implementation. */
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
static int
miniroute_sort_data(network_interrupt_arg_t *intrpt);
static void
miniroute_process_discovery(network_interrupt_arg_t *intrpt);
static void
miniroute_process_reply(network_interrupt_arg_t *intrpt);
int
miniroute_print_hdr(miniroute_header_t hdr);

/* Store network interrupts. Protected by disabling interrupts. */
static queue_t intrpt_buffer;
/* Signals when network interrupts need to be processed */
static semaphore_t intrpt_sig;
/* Caching routes */
static miniroute_cache_t route_cache;
/* Serial number of originating discovery packets */
static int discovery_id;

/* Performs any initialization of the miniroute layer, if required. */
void
miniroute_initialize()
{
    intrpt_buffer = queue_new();
    intrpt_sig = semaphore_create();
    route_cache = miniroute_cache_new(SIZE_OF_ROUTE_CACHE);
    if (NULL == intrpt_buffer || NULL == intrpt_sig || NULL == route_cache) {
        queue_free(intrpt_buffer);
        semaphore_destroy(intrpt_sig);
        return;
    }
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
#if MINIROUTE_CACHE_DEBUG == 1
    printf("Sending packing with header: \n");
    miniroute_print_hdr(&routing_hdr);
#endif

    if (sent_len < hdr_len)
        return -1;
    else
        return sent_len - hdr_len;
}

/* Pack the header of an outgoing data packet */
static int
miniroute_pack_data_hdr(miniroute_header_t hdr, network_address_t dest_address,
                        network_address_t next_hop_addr)
{
    miniroute_path_t item;
    hdr->routing_packet_type = ROUTING_DATA;
    pack_address(hdr->destination, dest_address);
    pack_unsigned_int(hdr->id, 0);
    pack_unsigned_int(hdr->ttl, MAX_ROUTE_LENGTH);
printf("Packing path.\n");
    miniroute_cache_get_by_addr(route_cache,  dest_address, &item);
    pack_unsigned_int(hdr->path_len, item->path_len);
    memcpy(hdr->path, item->path, 8 * item->path_len);
printf("Finished packing path.\n");
    network_address_copy(dest_address, next_hop_addr);
    return 0;
}

static void
miniroute_relay(network_interrupt_arg_t *intrpt)
{

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
    miniroute_header_t routing_hdr = (miniroute_header_t) intrpt->buffer;
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

    /* Free the interrupt if no protocol can handle it. */
    if (miniroute_sort_data(intrpt) == 1)
        free(intrpt);

    return;
}

/*
 * Pass the interrupt to the respective transport layer protocol.
 * Miniroute header in the interrupt should be stripped before it is passed
 * to this function.
 */
static int
miniroute_sort_data(network_interrupt_arg_t *intrpt)
{
    int status = 0;
    mini_header_t transport_hdr = (mini_header_t) intrpt->buffer;
    interrupt_level_t oldlevel = set_interrupt_level(DISABLED);
    switch (transport_hdr->protocol) {
    case PROTOCOL_MINIDATAGRAM:
        if (intrpt->size >= MINIMSG_HDRSIZE)
            minimsg_process(intrpt);
        break;
    case PROTOCOL_MINISTREAM:
        if (intrpt->size >= MINISOCKET_HDRSIZE)
            minisocket_process(intrpt);
        break;
    default:
        status = 1;
    }
    set_interrupt_level(oldlevel);

    return status;
}

/* Process a route discovery packet */
static void
miniroute_process_discovery(network_interrupt_arg_t *intrpt)
{

}

/* Process a reply packet */
static void
miniroute_process_reply(network_interrupt_arg_t *intrpt)
{

}

/* Print the miniroute header in a readable way for debugging */
int
miniroute_print_hdr(miniroute_header_t hdr)
{
    int i;
    int len;
    network_address_t dest;
    network_address_t hop;

    switch (hdr->routing_packet_type) {
    case ROUTING_ROUTE_DISCOVERY:
        printf("Discovery: ");
        break;
    case ROUTING_ROUTE_REPLY:
        printf("Reply    : ");
        break;
    case ROUTING_DATA:
        printf("Data     : ");
        break;
    default:
        printf("Unknown %d: ", hdr->routing_packet_type);
    }

    printf("id = %d, ", unpack_unsigned_int(hdr->id));
    printf("ttl = %d, ", unpack_unsigned_int(hdr->ttl));
    printf("len = %d\n", len = unpack_unsigned_int(hdr->path_len));

    unpack_address(hdr->destination, dest);
    printf("Destination: ");
    network_printaddr(dest);
    printf("\n");

    for (i = 0; i < len; ++i) {
        unpack_address(hdr->path[i], hop);
        printf("Hop %d: ", i);
        network_printaddr(hop);
        printf("\n");
    }

    return 0;
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


