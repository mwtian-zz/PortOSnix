#ifndef __NETWORK_H__
#define __NETWORK_H__

/*
 * network.h:
 *	Low-level network interface.
 *
 *	This interface defines a low-level network interface for sending and
 *	receiving packets between pseudo-network interfaces located on the 
 *      same or different hosts.
 */


#include "interrupts_private.h"
#include <sys/socket.h>
#include <netinet/in.h>

#define NETWORK_INTERRUPT_TYPE 2

#define MAX_NETWORK_PKT_SIZE	8192

#define MINIMSG_PORT 8086

#define BCAST_ENABLED 0
#define BCAST_USE_TOPOLOGY_FILE 0
#define BCAST_ADDRESS "192.168.1.255"
#define BCAST_LOOPBACK 0
#define BCAST_TOPOLOGY_FILE "topology.txt"


/* you should treat this as being opaque */
typedef unsigned int network_address_t[2];


typedef struct {
  network_address_t addr;
  char buffer[MAX_NETWORK_PKT_SIZE];
  int size;
} network_interrupt_arg_t;

/* Copy address "original" to address "copy".  We added this function
* to network.c so that we can treat network_address_t as "opaque" outside
* of network.h and network.c. */

/* zero the address, so as to make it invalid */
void network_address_blankify(network_address_t addr);

void
network_address_copy(network_address_t original, network_address_t copy);

/*Compare two addresses, return 1 if same, 0 otherwise.*/
int
network_address_same(network_address_t a, network_address_t b);

//print an address
void
network_printaddr(network_address_t addr);

void 
network_address_to_sockaddr(network_address_t addr, struct sockaddr_in *sin);

void 
sockaddr_to_network_address(struct sockaddr_in *sin, network_address_t addr);

/*
 * network_initialize should be called before clock interrupts start
 * happening (or with clock interrupts disabled).  The initialization
 * procedure returns 0 on success, -1 on failure.  The function
 * handler(data) is called when a network packet arrives. 
 *
 * n.b. you must call this function before you call any other network
 * functions, including network_translate_hostname().
 */
int
network_initialize(interrupt_handler_t network_handler);

/*
 * network_send_pkt returns the number of bytes sent if it was able to
 * successfully send the data.  Returns -1 otherwise.
 */
int
network_send_pkt(network_address_t dest_address,
		 int hdr_len, char * hdr,
		 int  data_len, char * data);


int
network_bcast_pkt(int hdr_len, char* hdr, int data_len, char* data);

/*
 * network_my_address returns the network_address that can be used
 * to send a packet to the caller's address space.  Note that
 * an address space can send a packet to itself by specifying the result of
 * network_get_my_address() as the dest_address to network_send_pkt.
 */
void
network_get_my_address(network_address_t my_address);

int
network_translate_hostname(char* hostname, network_address_t address);

/*
 * Compares network addresses. Returns 0 if different and 
 * nonzero if identical. 
 */
int 
network_compare_network_addresses(network_address_t addr1,
				  network_address_t addr2);

/*
 * write the network address in a human-readable way, into a buffer of length
 * "length"; will return -1 if the string is too short, else 0. the address
 * will be in the form "the.text.ip.address:port", e.g. "128.84.223.105:20".
 * n.b. the port is the NT port, not a miniport! 
 * 
 * for debugging.
*/
int
network_format_address(network_address_t address, char* string, int length);

/* 
 * only used for testing; normally, you should not have to call this
 * function. it should be called before network_initialize, and sets
 * the local UDP port to use for miniports, as well as the port number
 * to use for "remote" ports; by this mechanism, it's possible to run
 * a pair of processes on the same computer without their ports
 * conflicting. of course, this is a hack.
*/
void
network_udp_ports(short myportnum, short otherportnum);

/* 
 * set synthetic parameters for the network, used in network_send_pkt
 * to determine whether to drop or duplicate the packet to be sent 
 */
void
network_synthetic_params(double loss, double duplication);

/* for modifying the broadcast adjacency matrix. */
void
network_add_bcast_link(char* src, char* dest);

void
network_remove_bcast_link(char* src, char* dest);

#endif /*__NETWORK_H_*/

