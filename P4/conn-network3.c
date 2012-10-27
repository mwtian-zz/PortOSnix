/*
 *    conn-network test program 3
 *	  10 concurrent connections between two machines exchange big messages.
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
#define THREAD_COUNTER 10

int port[THREAD_COUNTER] = {80,81,82,83,84,85,86,87,88,89}; /* ports on which we do the communication */
char* hostname;
int thread_id[10] = {0,1,2,3,4,5,6,7,8,9};

int sender(int* arg);
int receiver(int* arg);		/* forward definitioan */

int server(int* arg)
{
    int i;

    for (i=0; i<THREAD_COUNTER; i++) {
        minithread_fork(sender,&thread_id[i]);
    }
    return 0;
}
int sender(int* arg)
{
    char buffer[BUFFER_SIZE];
    int i;
    int id;
    int bytes_sent;
    minisocket_t socket;
    minisocket_error error;

    id = *arg;
    socket = minisocket_server_create(port[id],&error);
    if (socket==NULL) {
        printf("*****GRADING: thread %d.Can't create the server. Error code: %d.\n",error);
        return 0;
    }

    /* Fill in the buffer with numbers from 0 to BUFFER_SIZE-1 */
    for (i=0; i<BUFFER_SIZE; i++) {
        buffer[i]=i%128;
    }

    /* send the message */
    bytes_sent=0;
    while (bytes_sent!=BUFFER_SIZE) {
        int trans_bytes=
            minisocket_send(socket,buffer+bytes_sent,
                            BUFFER_SIZE-bytes_sent, &error);
        printf("******GRADING: thread %d. Sent %d bytes.\n", id, trans_bytes);
        if (trans_bytes==-1) {
            printf("*****GRADING: thread %d. Sending error. Code: %d.\n", id, error);
            return 0;
        }
        bytes_sent+=trans_bytes;
    }
    printf("*****GRADING: thread %d. all data sent successfully\n", id);
    /* close the connection */
    minisocket_close(socket);
    return 0;
}

int client(int* arg)
{
    int i;

    for (i=0; i<THREAD_COUNTER; i++) {
        minithread_fork(receiver,&thread_id[i]);
    }

    return 0;
}

int receiver(int* arg)
{
    char buffer[BUFFER_SIZE];
    int i;
    int id;
    int bytes_received;
    network_address_t address;
    minisocket_t socket;
    minisocket_error error;
    int received_bytes;

    id = *arg;

    network_translate_hostname(hostname, address);
    /* create a network connection to the remote machine */
    socket = minisocket_client_create(address, port[id],&error);
    if (socket==NULL) {
        printf("can't create the client create, error: %d.\n",error);
        return 0;
    } else {
        printf("*****GRADING: thread %d. Server starts \n", id);
    }

    /* receive the message */
    bytes_received=0;
    while (bytes_received!=BUFFER_SIZE) {
        received_bytes = BUFFER_SIZE-bytes_received;
        received_bytes = minisocket_receive(socket,buffer+bytes_received,received_bytes, &error);
        if (received_bytes<0) {
            printf("*****GRADING: thread %d. Receiving error. Code: %d\n", id, error);
            return 0;
        }
        /* test the information received */
        for (i=0; i<received_bytes; i++) {
            if (buffer[bytes_received+i]!=((bytes_received+i)%128)) {
                printf("*****GRADING: thread %d. The %d'th byte received is wrong.\n", id, bytes_received+i);
            }
        }
        bytes_received+=received_bytes;
    }

    printf("*****GRADING: thread %d. All bytes received.\n",id);

    minisocket_close(socket);

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
        minithread_system_initialize(server, NULL);
    }
    return 0;
}
