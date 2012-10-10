/* network test program 4

   local loopback test: similar to network3.c, but in reverse: two senders send to one receiver.

   USAGE: ./minithread <port>
*/

#include "defs.h"
#include "minithread.h"
#include "minimsg.h"
#include "synch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define BUFFER_SIZE 256
#define MAX_COUNT 80

static miniport_t port;
static network_address_t my_address;

int receive(int* arg) {
  char buffer[BUFFER_SIZE];
  int length;
  int i;
  miniport_t from;

  for (i=0; i<2*MAX_COUNT; i++) {
    length = BUFFER_SIZE;
    minimsg_receive(port, &from, buffer, &length);
    printf("%s", buffer);
    miniport_destroy(from);
  }

  return 0;
}

int transmit2(int* arg) {
  char buffer[BUFFER_SIZE];
  int length;
  int i;
  miniport_t write_port;

  write_port = miniport_create_bound(my_address, 0);

  for (i=0; i<MAX_COUNT; i++) {
    printf("Sending packet %d from sender 2.\n", i+1);
    sprintf(buffer, "Count from sender 2 is %d.\n", i+1);
    length = strlen(buffer) + 1;
    minimsg_send(port, write_port, buffer, length);
    minithread_yield();
  }

  return 0;
}

int transmit1(int* arg) {
  char buffer[BUFFER_SIZE];
  int length;
  int i;
  miniport_t write_port;

  network_get_my_address(my_address);

  port = miniport_create_unbound(0);
  write_port = miniport_create_bound(my_address, 0);

  minithread_fork(transmit2, NULL);
  minithread_fork(receive, NULL);

  for (i=0; i<MAX_COUNT; i++) {
    printf("Sending packet %d from sender 1.\n", i+1);
    sprintf(buffer, "Count from sender 1 is %d.\n", i+1);
    length = strlen(buffer) + 1;
    minimsg_send(port, write_port, buffer, length);
    minithread_yield();
  }

  return 0;
}

int
main(int argc, char** argv) {
  short fromport;
  if (argc < 2)
        return -1;
  fromport = atoi(argv[1]);
  network_udp_ports(fromport,fromport);
  minithread_system_initialize(transmit1, NULL);
  return 0;
}
