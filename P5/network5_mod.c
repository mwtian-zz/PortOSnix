/* network test program 5

   send a stream of messages from one computer to another;
   this test may hang since the message layer (built on UDP) is unreliable, so
   if a message is lost, the receiver will block indefinitely.

   USAGE: ./minithread <souceport> <destport> [<hostname>]

   sourceport = udp port to listen on.
   destport   = udp port to send to.

   if no hostname is supplied, will function as the receiver; if a hostname is
   given, will send to that hostname. receiver must be running first!
*/

#include "defs.h"
#include "minithread.h"
#include "minimsg.h"
#include "synch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 256
#define MAX_COUNT 100


static char* hostname;

int receive(int* arg)
{
    char buffer[BUFFER_SIZE];
    int length;
    int i;
    miniport_t port;
    miniport_t from;

    port = miniport_create_unbound(1);

    for (i=0; i<MAX_COUNT; i++) {
        length = BUFFER_SIZE;
        minimsg_receive(port, &from, buffer, &length);
        printf("%s", buffer);
        miniport_destroy(from);
    }
    printf("Receive finished.\n");
    return 0;
}

int transmit(int* arg)
{
    char buffer[BUFFER_SIZE];
    int length;
    int i;
    network_address_t addr;
    miniport_t port;
    miniport_t dest;

    AbortOnCondition(network_translate_hostname(hostname, addr) < 0,
                     "Could not resolve hostname, exiting.");

    port = miniport_create_unbound(0);
    dest = miniport_create_bound(addr, 1);

    for (i=0; i<MAX_COUNT; i++) {
        printf("Sending packet %d.\n", i+1);
        sprintf(buffer, "Count is %d.\n", i+1);
        length = strlen(buffer) + 1;
        minimsg_send(port, dest, buffer, length);
    }
    printf("Send finished.\n");
    return 0;
}

int
main(int argc, char** argv)
{

    short fromport, toport;
    fromport = atoi(argv[1]);
    toport = atoi(argv[2]);
    network_udp_ports(fromport,toport);

    if (argc > 3) {
        hostname = argv[3];
        minithread_system_initialize(transmit, NULL);
    } else {
        minithread_system_initialize(receive, NULL);
    }

    return 0;
}
