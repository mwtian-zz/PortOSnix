/******************************************************************************
 * Usage:
 * minithreads : Start up as a minisocket server
 * minithreads <remote name> : Start up trying to connect to the remote server
 *****************************************************************************/

#include "defs.h"

#include "read.h"
#include "minisocket.h"
#include "minithread.h"

#define BUFFER_SIZE 256

/* File scope variables */
static char *remote_name;
static minisocket_t connection;
static minisocket_error error;
static int port_num = 7272;

/* File scope functions */
static int receiver(int* arg);
static int sender(int *arg);

static int
receiver(int* arg)
{
    int len;
    char buffer[BUFFER_SIZE];
    network_address_t dest;

    if (NULL != remote_name) {
        network_translate_hostname(remote_name, dest);
        connection = minisocket_client_create(dest, port_num, &error);
    } else {
        connection = minisocket_server_create(port_num, &error);
    }

    if (NULL == connection) {
        printf("Connection error.\n");
        return -1;
    } else {
        minithread_fork(sender, NULL);
        printf("Connection successful. Type and enter to send messages.\n");
    }

    while (1) {
        buffer[BUFFER_SIZE] = '\0';
        len = minisocket_receive(connection, buffer, BUFFER_SIZE - 1, &error);
        buffer[len] = '\0';
        printf("[Received message]: %s", buffer);
    }

    return 0;
}

static int
sender(int *arg)
{
    int len = BUFFER_SIZE;
    char buffer[BUFFER_SIZE];

    while (1) {
        len = miniterm_read(buffer, BUFFER_SIZE - 1);
        minisocket_send(connection, buffer, len + 1, &error);
    }

    return 0;
}

int
main(int argc, char** argv)
{
    if (argc > 1)
        remote_name = argv[1];

    minithread_system_initialize(receiver, NULL);

    return 0;
}
