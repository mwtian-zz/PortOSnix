#include "defs.h"

#include "queue.h"
#include "queue_wrap.h"
#include "synch.h"
#include "minithread.h"
#include "miniroute.h"
#include "miniheader.h"
#include "minimsg.h"
#include "minisocket.h"

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
    if (NULL == intrpt_sig)
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

    if (miniroute_pack_data_hdr(&routing_hdr, dest_address, next_hop_addr) == 0) {
        sent_len = network_send_pkt(dest_address, MINIROUTE_HDRSIZE,
                                    (char*)&routing_hdr, total_len, total_data);
    }

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
    network_address_copy(hdr->destination, dest_address);
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


int
miniroute_buffer_intrpt(network_interrupt_arg_t *intrpt)
{
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

}

static void
miniroute_process_discovery(network_interrupt_arg_t *intrpt)
{

}

static void
miniroute_process_reply(network_interrupt_arg_t *intrpt)
{

}
