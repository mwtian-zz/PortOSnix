/* network test program 3

   local loopback test: spawns three threads and creates two ports. one thread acts as the sender
   and sends pairs of messages, one to each port, in a loop, with yields in
   between. each of the other threads is assigned a port, and reads messages
   out of it. both ports are local.

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
#define MAX_COUNT 100

static miniport_t port1;
static miniport_t port2;

int receive1(int* arg) {
  char buffer[BUFFER_SIZE];
  int length;
  int i;
  miniport_t from;

  for (i=0; i<MAX_COUNT; i++) {
    length = BUFFER_SIZE;
    minimsg_receive(port1, &from, buffer, &length);
    printf("%s", buffer);
    miniport_destroy(from);
  }

  return 0;
}

int receive2(int* arg) {
  char buffer[BUFFER_SIZE];
  int length;
  int i;
  miniport_t from;

  for (i=0; i<MAX_COUNT; i++) {
    length = BUFFER_SIZE;
    minimsg_receive(port2, &from, buffer, &length);
    printf("%s", buffer);
    miniport_destroy(from);
  }

  return 0;
}

int transmit(int* arg) {
  char buffer[BUFFER_SIZE];
  int length;
  int i;
  minithread_t receiver1;
  minithread_t receiver2;
  miniport_t write_port1;
  miniport_t write_port2;
  network_address_t my_address;

  network_get_my_address(&my_address);

  port1 = miniport_create_unbound(0);
  port2 = miniport_create_unbound(1);
  write_port1 = miniport_create_bound(my_address, 0);
  write_port2 = miniport_create_bound(my_address, 1);

  receiver1 = minithread_fork(receive1, NULL);
  receiver2 = minithread_fork(receive2, NULL);

  for (i=0; i<MAX_COUNT; i++) {
    printf("Sending packet %d to receiver 1.\n", i+1);
    sprintf(buffer, "Count for receiver 1 is %d.\n", i+1);
    length = strlen(buffer) + 1;
    minimsg_send(port1, write_port1, buffer, length);
    minithread_yield();

    printf("Sending packet %d to receiver 2.\n", i+1);
    sprintf(buffer, "Count for receiver 2 is %d.\n", i+1);
    length = strlen(buffer) + 1;
    minimsg_send(port2, write_port2, buffer, length);
  }

  return 0;
}

main(int argc, char** argv) {
  short fromport;
  fromport = atoi(argv[1]);
  network_udp_ports(fromport,fromport);
  minithread_system_initialize(transmit, NULL);
}
