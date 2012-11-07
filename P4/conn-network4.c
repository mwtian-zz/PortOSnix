/*
 *    conn-network test program 4
 *    Close connection while there are threads receiving, see if they terminate.
 *    usage: conn-network3 [<hostname>]
 *    if no hostname is supplied, server will be run
 *    if a hostname is given, the client application will be run
 *    Make experiments with different values for BUFFER_SIZE
*/

#include "minithread.h"
#include "minisocket.h"
#include "synch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>


#define BUFFER_SIZE 100000
#define RECEIVER_SIZE 1000
#define THREAD_COUNTER 10

int port = 80;   /* ports on which we do the communication */
char* hostname;
int thread_id[10] = {0,1,2,3,4,5,6,7,8,9};
minisocket_t server_socket;
minisocket_t recv_skt;

int sender(int* arg);
int receiver(int* arg);		/* forward definitioan */



int
create_server(int *arg) {
    minisocket_error error;
    int i = 0;
    printf("Server waits.\n");
    server_socket = minisocket_server_create(port,&error);
    printf("Server created.\n");
    minithread_fork(sender, &thread_id[i]);

	return 0;
}

int sender(int* arg)
{
    char buffer[BUFFER_SIZE];
    int i;
    int id = *arg;
    int bytes_sent;
	minisocket_error error;

    if (server_socket==NULL) {
        printf("Sender NULL.\n");
        return 0;
    }

    /* Fill in the buffer with numbers from 0 to BUFFER_SIZE-1 */
    for (i=0; i<BUFFER_SIZE; i++) {
        buffer[i] = i % 128;
    }

    /* send the message */
    bytes_sent=0;
    while (bytes_sent != BUFFER_SIZE) {
        int trans_bytes =
            minisocket_send(server_socket,buffer+bytes_sent,
                            BUFFER_SIZE-bytes_sent, &error);
        printf("thread %d. Sent %d bytes.\n", id, trans_bytes);
        if (trans_bytes==-1) {
            printf("thread %d. Sending error. Code: %d.\n", id, error);
            return 0;
        }
        bytes_sent+=trans_bytes;
    }
    /* close the connection */
    minisocket_close(server_socket);
    return 0;
}

int client(int* arg)
{
    int i;
    minisocket_error error;
    network_address_t address;

    network_translate_hostname(hostname, address);
    recv_skt = minisocket_client_create(address, port, &error);
    for (i=0; i<THREAD_COUNTER; i++) {
        minithread_fork(receiver,&thread_id[i]);
    }

    return 0;
}

int receiver(int* arg)
{
    char buffer[BUFFER_SIZE];
    int id = *arg;
    int bytes_received;
    minisocket_error error;
    int received_bytes;

    /* receive the message */
    bytes_received=0;
    while (bytes_received != BUFFER_SIZE) {
        received_bytes = BUFFER_SIZE-bytes_received;
        received_bytes = minisocket_receive(recv_skt, buffer + bytes_received,
                                            RECEIVER_SIZE, &error);
        if (received_bytes<0) {
            printf("thread %d. Terminated. Code: %d\n", id, error);
            return 0;
        }
        bytes_received+=received_bytes;
    }
    printf("thread %d. Terminated. Success.\n");
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
        minithread_system_initialize(client, NULL);
    } else {
        minithread_system_initialize(create_server, NULL);
    }
    return 0;
}
