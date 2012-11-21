/* network test 8

   Based on network6.c.
   Send long messages between two processes.

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
#include "minimsg.h"
#include "minimsg_private.h"
#include "network.h"
#include "synch.h"

#define MAX_COUNT 1000000
#define SHOW_PROGRESS 1

static char* hostname;
static char sendbuffer[MINIMSG_MAX_MSG_SIZE + 10];
int receive_first(int* arg)
{
    char recvbuffer[MINIMSG_MAX_MSG_SIZE];
    int length;
    int i;
    miniport_t port;
    miniport_t from;

    port = miniport_create_unbound(1);

    for (i=0; i<MAX_COUNT; i++) {
        length = MINIMSG_MAX_MSG_SIZE;
        minimsg_receive(port, &from, recvbuffer, &length);
        if (SHOW_PROGRESS)
            printf("Receiving %d packet.\n", i+1);
        miniport_destroy(from);
    }

    printf("Receive-first finished.\n");

    return 0;
}

int transmit_first(int* arg)
{
    int i;
    network_address_t addr;
    miniport_t port;
    miniport_t dest;

    AbortOnCondition(network_translate_hostname(hostname, addr) < 0,
                     "Could not resolve hostname, exiting.");

    port = miniport_create_unbound(0);
    dest = miniport_create_bound(addr, 1);

    for (i=0; i<MAX_COUNT; i++) {
        if (SHOW_PROGRESS)
            printf("Sending packet %d.\n", i+1);
        if (minimsg_send(port, dest, sendbuffer, MINIMSG_MAX_MSG_SIZE) == -1)
            printf("Error on minimsg_send.\n");
    }

    printf("Send-first finished.\n");

    return 0;
}

int
main(int argc, char** argv)
{
    short fromport, toport;
    int i;
    if (argc < 3)
        return -1;
    fromport = atoi(argv[1]);
    toport = atoi(argv[2]);
    network_udp_ports(fromport,toport);

    for (i = 0; i < MINIMSG_MAX_MSG_SIZE; ++i)
        sendbuffer[i] = '_';
    sendbuffer[MINIMSG_MAX_MSG_SIZE - 1] = '\0';

    if (argc > 3) {
        hostname = argv[3];
        minithread_system_initialize(transmit_first, NULL);
    } else {
        minithread_system_initialize(receive_first, NULL);
    }
    return 0;
}
