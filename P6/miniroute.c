#include "defs.h"

#include "alarm.h"
#include "interrupts.h"
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
static miniroute_path_t
miniroute_discover_path(network_address_t dest);
static void
miniroute_pack_discovery_hdr(miniroute_header_t hdr, network_address_t dest);
static void
miniroute_pack_reply_hdr(miniroute_header_t hdr, int id, miniroute_path_t path);
static void
miniroute_pack_data_hdr(miniroute_header_t hdr, miniroute_path_t path);
static void
miniroute_pack_hdr_from_path(miniroute_header_t hdr, miniroute_path_t path);
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

/* Address of local machine */
network_address_t hostaddr;

/* Store network interrupts. Protected by disabling interrupts. */
static queue_t intrpt_buffer;
/* Signals when network interrupts need to be processed */
static semaphore_t intrpt_sig;
/* Allows only one route discovery process at a time */
static semaphore_t discovery_mutex;
/* Signals completion of discovery process */
static semaphore_t discovery_sig;
/* Result of discovery process */
//static discovery_process_status_t discovery_status;
/* Alarm for discovery timeout */
static int discovery_alarm;
/* Result of discovery process */
static miniroute_path_t discovered_path;
/* Routes cache*/
static miniroute_cache_t route_cache;
/* Network discovery packets cache */
static miniroute_cache_t disc_cache;
/* Serial number of originating discovery packets */
static int discovery_id;

/* Performs any initialization of the miniroute layer, if required. */
void
miniroute_initialize()
{
    discovery_alarm = -1;

    intrpt_buffer = queue_new();
    intrpt_sig = semaphore_create();
    discovery_mutex = semaphore_create();
    discovery_sig = semaphore_create();

    route_cache = miniroute_cache_new(65536, SIZE_OF_ROUTE_CACHE, MINIROUTE_CACHED_ROUTE_EXPIRE);
    disc_cache = miniroute_cache_new(65536, SIZE_OF_ROUTE_CACHE, MINIROUTE_CACHED_ROUTE_EXPIRE * 10);

    if (NULL == intrpt_buffer || NULL == intrpt_sig || NULL == route_cache
            || NULL == discovery_mutex || NULL == discovery_sig) {
        queue_free(intrpt_buffer);
        semaphore_destroy(intrpt_sig);
        semaphore_destroy(discovery_mutex);
        semaphore_destroy(discovery_sig);
        miniroute_cache_destroy(route_cache);
        return;
    }

    semaphore_initialize(intrpt_sig, 0);
    semaphore_initialize(discovery_mutex, 1);
    semaphore_initialize(discovery_sig, 0);

    network_get_my_address(hostaddr);

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
    struct routing_header routing_hdr;
    char *total_data = malloc(total_len);
    miniroute_path_t path = NULL;

    /* Find route if it is not in cache */
    semaphore_P(discovery_mutex);
    miniroute_cache_get_by_addr(route_cache,  dest_address, (void**)&path);

    if (NULL == path || path->exp_time < ticks) {
#if MINIROUTE_CACHE_DEBUG == 1
        printf("Address not found, discovering path.\n");
#endif
        path = miniroute_discover_path(dest_address);
    }

    semaphore_V(discovery_mutex);
    if (NULL != path && NULL != (total_data = malloc(total_len))) {
        /* Pack data and miniroute header */
        memcpy(total_data, hdr, hdr_len);
        memcpy(total_data + hdr_len, data, data_len);

        miniroute_pack_data_hdr(&routing_hdr, path);
        sent_len = network_send_pkt(path->hop[1], MINIROUTE_HDRSIZE,
                                    (char*)&routing_hdr, total_len, total_data);
    }
#if MINIROUTE_DEBUG == 1
    printf("Network packet sent.\n");
#endif
#if MINIROUTE_CACHE_DEBUG == 1
    printf("Sending data with header: \n");
    miniroute_print_hdr(&routing_hdr);
#endif

    if (sent_len < hdr_len)
        return -1;
    else
        return sent_len - hdr_len;
}

/*
 * Find path to the destination. Return a pointer to a miniroute_path struct,
 * or NULL if no route can be found.
 */
static miniroute_path_t
miniroute_discover_path(network_address_t dest)
{
    miniroute_path_t path = NULL;
    struct routing_header routing_hdr;
    miniroute_disc_hist_t disc;

    miniroute_pack_discovery_hdr(&routing_hdr, dest);
    disc = miniroute_dischist_from_hdr(&routing_hdr);
    miniroute_cache_put_item(disc_cache, disc);

#if MINIROUTE_CACHE_DEBUG == 1
    printf("Broadcasting discovery packet with header: \n");
    miniroute_print_hdr(&routing_hdr);
#endif
    network_bcast_pkt(MINIROUTE_HDRSIZE, (char*)&routing_hdr, 0, NULL);
    discovery_alarm = register_alarm(12000, semaphore_Signal, discovery_sig);
    semaphore_P(discovery_sig);
    /* Success when alarm is disabled by receiving a reply */
    if (-1 == discovery_alarm) {
        path = discovered_path;
        discovered_path = NULL;
    }
    return path;
}

/* Cancel discovery if it is in progress */
static void
miniroute_discovery_cancel()
{
    if (discovery_alarm > -1) {
        deregister_alarm(discovery_alarm);
        discovery_alarm = -1;
        semaphore_V(discovery_sig);
    }
}

/* Pack header for discovery packets */
static void
miniroute_pack_discovery_hdr(miniroute_header_t hdr, network_address_t dest)
{
    hdr->routing_packet_type = ROUTING_ROUTE_DISCOVERY;
    pack_address(hdr->destination, dest);
    pack_unsigned_int(hdr->id, discovery_id++);
    pack_unsigned_int(hdr->ttl, MAX_ROUTE_LENGTH);
    pack_unsigned_int(hdr->path_len, 1);
    pack_address(hdr->path[0], hostaddr);
}

/* Pack header for reply packets */
static void
miniroute_pack_reply_hdr(miniroute_header_t hdr, int id, miniroute_path_t path)
{
    hdr->routing_packet_type = ROUTING_ROUTE_REPLY;
    pack_address(hdr->destination, path->addr);
    pack_unsigned_int(hdr->id, id);
    pack_unsigned_int(hdr->ttl, MAX_ROUTE_LENGTH);
    miniroute_pack_hdr_from_path(hdr, path);
}

/* Pack the header of an outgoing data packet */
static void
miniroute_pack_data_hdr(miniroute_header_t hdr, miniroute_path_t path)
{

    hdr->routing_packet_type = ROUTING_DATA;
    pack_address(hdr->destination, path->addr);
    pack_unsigned_int(hdr->id, 0);
    pack_unsigned_int(hdr->ttl, MAX_ROUTE_LENGTH);
    miniroute_pack_hdr_from_path(hdr, path);
}

/* Pack the routing portion of the header */
static void
miniroute_pack_hdr_from_path(miniroute_header_t hdr, miniroute_path_t path)
{
    unsigned int i;
    pack_unsigned_int(hdr->ttl, MAX_ROUTE_LENGTH);
    pack_unsigned_int(hdr->path_len, path->path_len);
    for (i = 0; i < path->path_len; ++i)
        pack_address(hdr->path[i], path->hop[i]);
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
    int ttl;
    network_address_t dest;
    while (1) {
        miniroute_wait_for_intrpt(&intrpt);
#if MINIROUTE_DEBUG == 1
        printf("Sorting Network packet.\n");
#endif
        header = (miniroute_header_t) intrpt->buffer;
        ttl = unpack_unsigned_int(header->ttl);

        if (intrpt->size < MINIROUTE_HDRSIZE) {
            free(intrpt);
        } else if (network_address_same(dest, hostaddr) != 1 && ttl <= 0) {
            free(intrpt);
        } else {
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

/* Relay the message to the next hop using network_send_pkt */
static void
miniroute_relay(network_interrupt_arg_t *intrpt)
{
    int i;
    int len;
    int ttl;
    miniroute_header_t routing_hdr = (miniroute_header_t) intrpt->buffer;
    network_address_t hop;

    len = unpack_unsigned_int(routing_hdr->path_len);
    for (i = 0; i < len; ++i) {
        unpack_address(routing_hdr->path[i], hop);
        if (network_address_same(hostaddr, hop) == 1)
            break;
    }
    if (i + 1 < len) {
        i++;
        ttl = unpack_unsigned_int(routing_hdr->ttl);
        pack_unsigned_int(routing_hdr->ttl, --ttl);
        unpack_address(routing_hdr->path[i], hop);

#if MINIROUTE_CACHE_DEBUG == 1
        printf("Relaying header\n");
        miniroute_print_hdr(routing_hdr);
#endif
        network_send_pkt(hop, 0, NULL, intrpt->size, intrpt->buffer);
    }
    free(intrpt);
}

static void
miniroute_process_data(network_interrupt_arg_t *intrpt)
{
    int i;
    miniroute_header_t routing_hdr = (miniroute_header_t) intrpt->buffer;
    network_address_t dest;
    unpack_address(routing_hdr->destination, dest);

#if MINIROUTE_DEBUG == 1
    printf("Processing data in Network packet.\n");
#endif
    /* Relay packets if it is intended for others */
    if (network_address_same(dest, hostaddr) != 1)
        miniroute_relay(intrpt);
    else {
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
    }

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
    miniroute_header_t hdr = (miniroute_header_t) intrpt->buffer;
    network_address_t orig;
    network_address_t dest;
    int id;
    int len;
    int ttl;
    miniroute_path_t path;
    miniroute_disc_hist_t disc;

#if MINIROUTE_CACHE_DEBUG == 1
    printf("Received discovery packet with header: \n");
    miniroute_print_hdr(hdr);
#endif
    id = unpack_unsigned_int(hdr->id);
    len = unpack_unsigned_int(hdr->path_len);
    pack_unsigned_int(hdr->path_len, ++len);
    pack_address(hdr->path[len - 1], hostaddr);
    unpack_address(hdr->path[0], orig);
    unpack_address(hdr->destination, dest);

    if (network_address_same(dest, hostaddr) != 1) {
        miniroute_cache_get_by_addr(disc_cache, orig, (void**)&disc);
        if (!(NULL != disc && disc->id ==  id)) {
            disc = miniroute_dischist_from_hdr(hdr);
            miniroute_cache_put_item(disc_cache, disc);
            ttl = unpack_unsigned_int(hdr->ttl);
            pack_unsigned_int(hdr->ttl, --ttl);
            network_bcast_pkt(0, NULL, intrpt->size, intrpt->buffer);
        }
    } else {
        path = miniroute_path_from_hdr(hdr);
        miniroute_cache_put_item(route_cache, path);
        miniroute_pack_reply_hdr(hdr, id, path);
#if MINIROUTE_CACHE_DEBUG == 1
        printf("Processed discovery packet, replying with header: \n");
        miniroute_print_hdr(hdr);
#endif
        network_send_pkt(path->hop[1], MINIROUTE_HDRSIZE, (char*)hdr, 0, NULL);
    }

    free(intrpt);
}

/* Process a reply packet */
static void
miniroute_process_reply(network_interrupt_arg_t *intrpt)
{
    miniroute_header_t hdr = (miniroute_header_t) intrpt->buffer;
    network_address_t dest;
    unpack_address(hdr->destination, dest);

    /* Relay packets if it is intended for others */
    if (network_address_same(dest, hostaddr) != 1) {
        miniroute_relay(intrpt);
    } else {
        discovered_path = miniroute_path_from_hdr(hdr);
        miniroute_cache_put_item(route_cache, (miniroute_item_t*)discovered_path);
        free(intrpt);
        miniroute_discovery_cancel();
    }
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



