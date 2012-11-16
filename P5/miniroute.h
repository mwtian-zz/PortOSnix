#ifndef _MINIROUTE_H_
#define _MINIROUTE_H_

#include "minimsg.h"

#define MAX_ROUTE_LENGTH 20
#define SIZE_OF_ROUTE_CACHE 20
#define MINIROUTE_HDRSIZE (sizeof(struct routing_header))

enum routing_packet_type {
    ROUTING_DATA=0,
    ROUTING_ROUTE_DISCOVERY=1,
    ROUTING_ROUTE_REPLY=2
};

struct routing_header {
    char routing_packet_type;		/* the type of routing packet */
    char destination[8];			/* ultimate destination of routing packet */
    char id[4];						/* identifier value for this broadcast (only applicable for discovery and route reply msgs, 0 otherwise */
    char ttl[4];					/* number of hops until packet is destroyed (time to live) */

    char path_len[4];				/* length of route, indicates the number of valid entries in the path array.
									   This should be smaller than MAX_ROUTE_LENGTH */
    char path[MAX_ROUTE_LENGTH][8];	/* contains the packed network addresses of each node in the route.
									   The address of the source is stored in the first position, and the
									   address of the destination is stored in the last position. */
};
typedef struct routing_header *miniroute_header_t;


/* Performs any initialization of the miniroute layer, if required. */
void miniroute_initialize();

/*
 * miniroute_send_pkt returns the number of bytes sent (which should be the sum of the user's header length and
 * data length, so this does not include the miniroute header length) if it was able to successfully send the
 * data. Returns -1 otherwise.
 *
 * Before the packet can be sent a route has to be discovered. The route cache is consulted for this purpose
 * and if a route younger than 3 seconds is not found, the route discovery protocol is run. The function should
 * return only when the packet is successfully sent (running the route discovery protocol blocks the thread
 * calling this function). Multiple concurrent calls to route a packet to the same destination on a cold cache
 * should generate only one broadcast discovery packet for this destination.
 *
 * All calls to network_send_pkt in the previous code should be replaced with calls to this function instead.
 *
 */
int miniroute_send_pkt(network_address_t dest_address, int hdr_len, char* hdr, int data_len, char* data);


/* Buffer the interrupt and signal the  processing threaed */
int miniroute_buffer_intrpt(network_interrupt_arg_t *intrpt);

/*
 * hash function that generates an unsigned short integer value from a given network address. This value will
 * range between 0 and 65520 (almost the full range of an unsigned short), and you must manually scale or
 * rehash this value into the range you want.
 *
 */
unsigned short hash_address(network_address_t address);


#endif /* _MINIROUTE_H_ */
