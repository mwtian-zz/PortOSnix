/* network test program 1

   local loopback test: sends and then receives one message on the same machine.

USAGE: ./minithread <port>

*/

#include "minithread.h"
#include "minimsg.h"
#include "synch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>


#define BUFFER_SIZE 256


miniport_t listen_port;
miniport_t send_port;

char text[] = "Hello, world!\n";
int textlen=14;

int thread(int* arg)
{
    char buffer[BUFFER_SIZE];
    int length = BUFFER_SIZE;
    miniport_t from;
    network_address_t my_address;
printf("Start.\n");
    network_get_my_address(my_address);

    listen_port = miniport_create_unbound(0);
    send_port = miniport_create_bound(my_address, 0);
printf("Before send.\n");
    minimsg_send(listen_port, send_port, text, textlen);
printf("Before receive.\n");
    minimsg_receive(listen_port, &from, buffer, &length);
printf("After receive.\n");
    printf("%s", buffer);

    return 0;
}

int
main(int argc, char** argv)
{
    short fromport;
    if (argc < 2)
        return -1;
    fromport = atoi(argv[1]);
    network_udp_ports(fromport,fromport);
    textlen = strlen(text) + 1;
    minithread_system_initialize(thread, NULL);

    return 0;
}
