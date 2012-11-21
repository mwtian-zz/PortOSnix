#include "defs.h"

#include "read.h"
#include "minisocket.h"
#include "minithread.h"

#define BUFFER_SIZE 256

int
msg_server(int* arg)
{
    int len = BUFFER_SIZE;
    char buffer[BUFFER_SIZE];
    minisocket_error error;
    minisocket_t server;

    server = minisocket_server_create(0, &error);

    while (1) {
        len = minisocket_receive(server, buffer, BUFFER_SIZE, &error);
        if (len == -1)
            break;
        if (len == BUFFER_SIZE)
            --len;
        buffer[len] = '\0';
        printf("Received message: %s\n", buffer);
    }

    return 0;
}

int
main()
{
    int len = BUFFER_SIZE;
    char buffer[BUFFER_SIZE];
    network_address_t dest;
    minisocket_error error;
    minisocket_t client;

    minithread_fork(msg_server, NULL);

    printf("Type in the destination machine name:\n");
    miniterm_read(buffer, len);
    network_translate_hostname(buffer, dest);
    client = minisocket_client_create(dest, 0, &error);

    while (1) {
        miniterm_read(buffer, len);
        minisocket_send(client, buffer, len, &error);
    }

    return 0;
}
