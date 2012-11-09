/* network test 7

   Based on 'network5.c' and 'network6.c'.
   Send malformed messages from one process to the other.

   USAGE: ./minithread <souceport> <destport> [<hostname>]

   sourceport = udp port to listen on.
   destport   = udp port to send to.

   if no hostname is supplied, will wait for a packet before sending the first
   packet; if a hostname is given, will send and then receive.
   the receive-first copy must be started first!
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "defs.h"
#include "minithread.h"
#include "miniheader.h"
#include "minimsg.h"
#include "minimsg_private.h"
#include "network.h"
#include "synch.h"

#define BUFFER_SIZE MINIMSG_MAX_MSG_SIZE
#define MAX_COUNT 10
#define NUM_TESTS 2

static char* hostname;

int receive_first(int* arg)
{
    char buffer[BUFFER_SIZE];
    int length;
    int i;
    miniport_t port;
    miniport_t from;

    port = miniport_create_unbound(1);

    for (i = 0; i < NUM_TESTS; ++i) {
        length = BUFFER_SIZE;
        minimsg_receive(port, &from, buffer, &length);
        printf("%s", buffer);
        miniport_destroy(from);
    }

    printf("Receive-first finished.\n");

    return 0;
}

int transmit_first(int* arg)
{
    char buffer[MINIMSG_MAX_MSG_SIZE + 10];
    int length = 0;
    int i;
    network_address_t hostaddr, targetaddr;
    miniport_t port;
    miniport_t dest;
    struct mini_header hdr;

    AbortOnCondition(network_translate_hostname(hostname, targetaddr) < 0,
                     "Could not resolve hostname, exiting.");

    port = miniport_create_unbound(0);
    dest = miniport_create_bound(targetaddr, 1);

    /* Form correct header */
    network_get_my_address(hostaddr);
    hdr.protocol = PROTOCOL_MINIDATAGRAM;
    pack_address(hdr.source_address, hostaddr);
    pack_unsigned_short(hdr.source_port, port->num);
    pack_address(hdr.destination_address, dest->bound.addr);
    pack_unsigned_short(hdr.destination_port, dest->bound.remote);

    /* Send packages with short but correct header and zero data */
    printf("Sending packages with short headers.\n");
    sprintf(buffer, "Receiving packages with short headers.\n");
    length = strlen(buffer) + 1;
    minimsg_send(port, dest, buffer, length);

    for (i = 0; i < MINIMSG_HDRSIZE; i++)
        network_send_pkt(targetaddr, i, (char*)&hdr, 0, buffer);

    /* Send packages to wrong ports */
    printf("Sending packages to wrong destination ports.\n");
    sprintf(buffer, "Receiving packages with wrong destination ports.\n");
    length = strlen(buffer) + 1;
    minimsg_send(port, dest, buffer, length);
    sprintf(buffer, "This message is sent to a wrong port.\n");
    length = strlen(buffer) + 1;
    minimsg_send(port, miniport_create_bound(targetaddr, 0),
                 buffer, length);
    minimsg_send(port, miniport_create_bound(targetaddr, MAX_UNBOUNDED),
                 buffer, length);
    minimsg_send(port, miniport_create_bound(targetaddr, MAX_UNBOUNDED + 1),
                 buffer, length);
    minimsg_send(port, miniport_create_bound(targetaddr, MIN_BOUNDED),
                 buffer, length);
    minimsg_send(port, miniport_create_bound(targetaddr, MAX_BOUNDED),
                 buffer, length);

    printf("Send-first finished.\n");

    return 0;
}

int
main(int argc, char** argv)
{
    short fromport, toport;
    if (argc < 3)
        return -1;
    fromport = atoi(argv[1]);
    toport = atoi(argv[2]);
    network_udp_ports(fromport,toport);

    if (argc > 3) {
        hostname = argv[3];
        minithread_system_initialize(transmit_first, NULL);
    } else {
        minithread_system_initialize(receive_first, NULL);
    }
    return 0;
}
