/*
 *	Implementation of minisockets.
 */

#include "defs.h"

#include "alarm.h"
#include "minisocket.h"
#include "minisocket_private.h"
#include "minithread.h"
#include "network.h"
#include "queue_wrap.h"
#include "queue.h"
#include "synch.h"
#include "miniheader.h"

static int
minisocket_server_init(network_interrupt_arg_t *intrpt, minisocket_t local);
static int
minisocket_transmit(minisocket_t socket, char msg_type, char *buf, int len);
static void
minisocket_acknowlege(minisocket_t socket);
static void
minisocket_retry_wait(minisocket_t socket, int delay);
static void
minisocket_retry_cancel(minisocket_t socket);
static void
minisocket_packhdr(mini_header_reliable_t header, minisocket_t socket,
                   char message_type);
static int
minisocket_validate(network_interrupt_arg_t *intrpt, minisocket_t local);
static int
minisocket_process_syn(network_interrupt_arg_t *intrpt, minisocket_t local);
static int
minisocket_process_synack(network_interrupt_arg_t *intrpt, minisocket_t local);
static int
minisocket_process_ack(network_interrupt_arg_t *intrpt, minisocket_t local);
static int
minisocket_process_fin(network_interrupt_arg_t *intrpt, minisocket_t local);
static int
minisocket_get_free_socket();
static int
minisocket_initialize_socket(int port, minisocket_error *error);
static void
minisocket_destroy(minisocket_t socket);


static minisocket_t minisocket[MINISOCKET_PORT_NUM];
static int socket_count = 0;
static int retry_delay[MINISOCKET_MAX_TRY];
static int win_size = 1;
static network_address_t hostaddr;
static semaphore_t port_count_mutex = NULL;
static semaphore_t port_array_mutex = NULL;
static semaphore_t source_port_mutex = NULL;

/* Initializes the minisocket layer. */
void
minisocket_initialize()
{
    int i;
    network_get_my_address(hostaddr);
    if ((port_count_mutex = semaphore_create()) != NULL) {
        semaphore_initialize(port_count_mutex, 1);
    }
    if ((port_array_mutex = semaphore_create()) != NULL) {
        semaphore_initialize(port_array_mutex, 1);
    }
    if ((source_port_mutex = semaphore_create()) != NULL) {
        semaphore_initialize(source_port_mutex, 1);
    }
    retry_delay[0] = MINISOCKET_INITIAL_TIMEOUT;
    for (i = 1; i < MINISOCKET_MAX_TRY; ++i)
        retry_delay[i] = 2 * retry_delay[i - 1];

}

/*
 * Listen for a connection from somebody else. When communication link is
 * created return a minisocket_t through which the communication can be made
 * from now on.
 *
 * The argument "port" is the port number on the local machine to which the
 * client will connect.
 *
 * Return value: the minisocket_t created, otherwise NULL with the errorcode
 * stored in the "error" variable.
 */
minisocket_t
minisocket_server_create(int port, minisocket_error *error)
{
    interrupt_level_t oldlevel;
    if (error != NULL) {
        *error = SOCKET_NOERROR;
    }
    /* Port out of range */
    if (port < MINISOCKET_MIN_SERVER || port > MINISOCKET_MAX_SERVER) {
        if (error != NULL) {
            *error = SOCKET_PORTOUTOFBOUND;
        }
        return NULL;
    }

    semaphore_P(source_port_mutex);
    /* Port already in use */
    if (minisocket[port] != NULL) {
        if (error != NULL) {
            *error = SOCKET_PORTINUSE;
        }
        semaphore_V(source_port_mutex);
        return NULL;
    }

    /* Create the socket */
    minisocket[port] = malloc(sizeof(struct minisocket));
    semaphore_V(source_port_mutex);

    /* Assume out of memory if malloc returns NULL */
    if (minisocket[port] == NULL) {
        if (error != NULL) {
            *error = SOCKET_OUTOFMEMORY;
        }
        return NULL;
    }

    /* Initialize socket with port number */
    if (minisocket_initialize_socket(port, error) == -1) {
        if (error != NULL) {
            *error = SOCKET_OUTOFMEMORY;
        }
        return NULL;
    }

    do {
        minisocket[port]->state = LISTEN;
        /* Wait for SYN from client */
        oldlevel = set_interrupt_level(DISABLED);
        semaphore_P(minisocket[port]->synchonize);
        set_interrupt_level(oldlevel);
        /* Woke up by minisocket_process_syn */
        if (SYNRECEIVED == minisocket[port]->state)
            minisocket_transmit(minisocket[port], MSG_SYNACK, NULL, 0);
        /* If SYNACK is received, state becomes ESTABLISHED */

    } while (minisocket[port]->state != ESTABLISHED);

    return minisocket[port];
}

/*
 * Initiate the communication with a remote site. When communication is
 * established create a minisocket through which the communication can be made
 * from now on.
 *
 * The first argument is the network address of the remote machine.
 *
 * The argument "port" is the port number on the remote machine to which the
 * connection is made. The port number of the local machine is one of the free
 * port numbers.
 *
 * Return value: the minisocket_t created, otherwise NULL with the errorcode
 * stored in the "error" variable.
 */
minisocket_t
minisocket_client_create(network_address_t addr, int port,
                         minisocket_error *error)
{
    int source_port_num;
    if (error != NULL) {
            *error = SOCKET_NOERROR;
    }
    /* Get a free source port number */
    source_port_num = minisocket_get_free_socket();
    /* Run out of free ports */
    if (source_port_num == -1 || source_port_num < MINISOCKET_MIN_CLIENT
            || source_port_num > MINISOCKET_MAX_CLIENT) {
        if (error != NULL) {
            *error = SOCKET_NOMOREPORTS;
        }
        return NULL;
    }

    minisocket[source_port_num] = malloc(sizeof(struct minisocket));
    if (minisocket[source_port_num] == NULL) {
        if (error != NULL) {
            *error = SOCKET_OUTOFMEMORY;
        }
        return NULL;
    }

    /* Initialize socket with source_port_num */
    if (minisocket_initialize_socket(source_port_num, error) == -1) {
        return NULL;
    }

    minisocket[source_port_num]->remote_port_num = port;
    network_address_copy(addr, minisocket[source_port_num]->addr);
    minisocket[source_port_num]->seq = 0;
    minisocket[source_port_num]->ack = 0;
    minisocket[source_port_num]->state = SYNSENT;

    /* Send syn to server */
    if (minisocket_transmit(minisocket[source_port_num], MSG_SYN, NULL, 0) == -1) {
        /* Connection to server fails, destroy socket */
        if (error != NULL) {
            *error = SOCKET_NOSERVER;
        }
        minisocket_destroy(minisocket[source_port_num]);
        return NULL;
    }

    return minisocket[source_port_num];
}


/* Initialize socket with port number, return -1 on failure, 0 on success*/
static int
minisocket_initialize_socket(int port, minisocket_error *error)
{
    minisocket_t socket = minisocket[port];
    if (NULL == socket) {
        *error = SOCKET_OUTOFMEMORY;
        return -1;
    }
    socket->local_port_num = port;
    socket->mutex = NULL;
    socket->synchonize = NULL;
    socket->retry = NULL;
    socket->receive = NULL;
    socket->data = NULL;
    socket->mutex = semaphore_create();
    socket->synchonize = semaphore_create();
    socket->retry = semaphore_create();
    socket->receive = semaphore_create();
    socket->data = queue_new();
    if (NULL == socket->mutex || NULL == socket->synchonize
            || NULL == socket->retry || NULL == socket->receive
            || NULL == socket->data) {
        minisocket[port] = NULL;
        *error = SOCKET_OUTOFMEMORY; /* Assume out of memory? */
        minisocket_destroy(socket);
        return -1;
    }
    semaphore_initialize(socket->mutex, 1);
    semaphore_initialize(socket->synchonize, 0);
    semaphore_initialize(socket->retry, 0);
    semaphore_initialize(socket->receive, 0);

    return 0;
}

/*
 * Get the next available socket, return -1 if none available
 */
static int
minisocket_get_free_socket()
{
    int num = -1;
    int i;

    semaphore_P(port_array_mutex);
    if (socket_count >= MINISOCKET_CLIENT_NUM) {
        semaphore_V(port_array_mutex);
        return num;
    }
    for (i = MINISOCKET_MIN_CLIENT; i <= MINISOCKET_MAX_CLIENT; i++) {
        if (minisocket[i] == NULL) {
            num = i;
            minisocket[i] = (minisocket_t)-1; /* Reserve this port number */
            semaphore_P(port_count_mutex);
            socket_count++;
            semaphore_V(port_count_mutex);
            break;
        }
    }
    semaphore_V(port_array_mutex);
    return num;
}

static void
minisocket_destroy(minisocket_t socket)
{
    if (socket != NULL) {
        minisocket[socket->local_port_num] = NULL;
        queue_free(socket->data);
        semaphore_destroy(socket->receive);
        semaphore_destroy(socket->mutex);
        free(socket);
        semaphore_P(port_count_mutex);
        socket_count--;
        semaphore_V(port_count_mutex);
    }
}

/*
 * Send a message to the other end of the socket.
 *
 * The send call should block until the remote host has ACKnowledged receipt of
 * the message.  This does not necessarily imply that the application has called
 * 'minisocket_receive', only that the packet is buffered pending a future
 * receive.
 *
 * It is expected that the order of calls to 'minisocket_send' implies the order
 * in which the concatenated messages will be received.
 *
 * 'minisocket_send' should block until the whole message is reliably
 * transmitted or an error/timeout occurs
 *
 * Arguments: the socket on which the communication is made (socket), the
 *            message to be transmitted (msg) and its length (len).
 * Return value: returns the number of successfully transmitted bytes. Sets the
 *               error code and returns -1 if an error is encountered.
 */
int
minisocket_send(minisocket_t socket, minimsg_t msg, int len,
                minisocket_error *error)
{
    int sent = 0, total_sent = 0, to_sent = len;

    if (socket == NULL || socket->state == CLOSED) {
        if (error != NULL) {
            *error = SOCKET_SENDERROR;
        }
        return -1;
    }

    semaphore_P(socket->mutex);
    while (to_sent > 0) {
        if (socket->state == CLOSED) {
            if (error != NULL) {
                *error = SOCKET_SENDERROR;
            }
            semaphore_V(socket->mutex);
            return -1;
        }
        if (to_sent > MINISOCKET_MAX_MSG_SIZE) {
            sent = minisocket_transmit(socket, MSG_ACK, msg + total_sent, MINISOCKET_MAX_MSG_SIZE);
        } else {
            sent = minisocket_transmit(socket, MSG_ACK, msg + total_sent, to_sent);
        }
        if (sent == -1) {
            if (error != NULL) {
                *error = SOCKET_SENDERROR;
            }
            semaphore_V(socket->mutex);
            return -1;
        }
        total_sent += sent;
        to_sent -= sent;
    }
    semaphore_V(socket->mutex);
    return total_sent;
}

/*
 * Receive a message from the other end of the socket. Blocks until
 * some data is received (which can be smaller than max_len bytes).
 *
 * Arguments: the socket on which the communication is made (socket), the memory
 *            location where the received message is returned (msg) and its
 *            maximum length (max_len).
 * Return value: -1 in case of error and sets the error code, the number of
 *           bytes received otherwise
 */
int
minisocket_receive(minisocket_t socket, minimsg_t msg, int max_len,
                   minisocket_error *error)
{
    int stored_len = 0;
    int received;
    network_interrupt_arg_t *intrpt;
    mini_header_reliable_t header;
    interrupt_level_t oldlevel;

    if (socket->state == CLOSED) {
        if (error != NULL) {
            *error = SOCKET_RECEIVEERROR;
        }
        return -1;
    }
    oldlevel = set_interrupt_level(DISABLED);
    semaphore_P(socket->receive);
    while (queue_wrap_dequeue(socket->data, (void**) &intrpt) == 0) {
        header = (mini_header_reliable_t) intrpt->buffer;
        received = intrpt->size - MINISOCKET_HDRSIZE;
        if (stored_len + received <= max_len) {
            memcpy(msg, header + 1, received);
            free(intrpt);
            stored_len += received;
        } else {
            queue_wrap_prepend(socket->data, intrpt);
            semaphore_V(socket->receive);
            break;
        }
    }
    set_interrupt_level(oldlevel);

    return stored_len;
}

/* Close a connection. If minisocket_close is issued, any send or receive should
 * fail.  As soon as the other side knows about the close, it should fail any
 * send or receive in progress. The minisocket is destroyed by minisocket_close
 * function.  The function should never fail.
 */
void
minisocket_close(minisocket_t socket)
{
    if (socket->state != ESTABLISHED) {
        return;
    }
    semaphore_P(socket->mutex);
    socket->state = FINSENT;
    socket->seq++;
    minisocket_transmit(socket, MSG_FIN, NULL, 0);
    socket->state = CLOSED;
    semaphore_V(socket->mutex);
    minisocket_destroy(socket);
}

/* Return -1 on failure */
static int
minisocket_transmit(minisocket_t socket, char msg_type, minimsg_t msg, int len)
{
    int i;
    struct mini_header_reliable header;
    if (!(MSG_ACK == msg_type && 0 == len))
        ++socket->seq;
    minisocket_packhdr(&header, socket, msg_type);
    for (i = 0; i < MINISOCKET_MAX_TRY; ++i) {
        network_send_pkt(socket->addr, MINISOCKET_HDRSIZE,
                                (char*)&header, len, msg);
        minisocket_retry_wait(socket, retry_delay[i]);
        if (-1 == socket->alarm)
            return len;
    }

    socket->alarm = -1;
    return -1;
}

/* seq, ack and message_type may get from socket */
static void
minisocket_packhdr(mini_header_reliable_t header, minisocket_t socket,
                   char message_type)
{
    header->protocol = PROTOCOL_MINISTREAM;
    pack_address(header->source_address, hostaddr);
    pack_unsigned_short(header->source_port, socket->local_port_num);
    pack_address(header->destination_address, socket->addr);
    pack_unsigned_short(header->destination_port, socket->remote_port_num);
    header->message_type = message_type;
    pack_unsigned_int(header->seq_number, socket->seq);
    pack_unsigned_int(header->ack_number, socket->ack);
}

static void
minisocket_retry_wait(minisocket_t socket, int delay)
{
    socket->alarm = register_alarm(delay, semaphore_Signal, socket->retry);
    semaphore_P(socket->retry);
}

static void
minisocket_retry_cancel(minisocket_t socket)
{
    deregister_alarm(socket->alarm);
    socket->alarm = -1;
    semaphore_V(socket->retry);
}

static void
minisocket_acknowlege(minisocket_t local)
{
    struct mini_header_reliable header;
    minisocket_packhdr(&header, local, MSG_ACK);
    network_send_pkt(local->addr, MINISOCKET_HDRSIZE, (char*)&header, 0, NULL);
}

int
minisocket_process(network_interrupt_arg_t *intrpt)
{
    minisocket_interrupt_status intrpt_status;
    mini_header_reliable_t header = (mini_header_reliable_t) intrpt->buffer;
    minisocket_t local;
    network_address_t local_addr;
    int local_num = unpack_unsigned_short(header->destination_port);
    int type = header->message_type;
    /* Sanity checks kept at minimum. */
    unpack_address(header->destination_address, local_addr);
    if (local_num > MINISOCKET_MAX_CLIENT || local_num < MINISOCKET_MIN_SERVER
            || NULL == minisocket[local_num]
            || network_address_same(hostaddr, local_addr) != 1) {
        free(intrpt);
        return -1;
    }
    local = minisocket[local_num];
    switch (type) {
    case MSG_SYN:
        intrpt_status = minisocket_process_syn(intrpt, local);
        break;
    case MSG_SYNACK:
        intrpt_status = minisocket_process_synack(intrpt, local);
        break;
    case MSG_ACK:
        intrpt_status = minisocket_process_ack(intrpt, local);
        break;
    case MSG_FIN:
        intrpt_status = minisocket_process_fin(intrpt, local);
        break;
    }

    if (INTERRUPT_PROCESSED == intrpt_status)
        free(intrpt);

    return intrpt_status;
}

static int
minisocket_validate(network_interrupt_arg_t *intrpt, minisocket_t local)
{
    mini_header_reliable_t header = (mini_header_reliable_t) intrpt->buffer;
    network_address_t remote_addr;
    int remote_num = unpack_unsigned_short(header->source_port);
    unpack_address(header->source_address, remote_addr);
    if (local->remote_port_num != remote_num
            || network_address_same(local->addr, remote_addr) != 1)
        return -1;
    return 0;
}

static int
minisocket_server_init(network_interrupt_arg_t *intrpt, minisocket_t local)
{
    mini_header_reliable_t header = (mini_header_reliable_t) intrpt->buffer;
    local->remote_port_num = unpack_unsigned_short(header->source_port);
    unpack_address(header->source_address, local->addr);
    local->ack = unpack_unsigned_int(header->seq_number);
    local->seq = 0;
    local->state = SYNRECEIVED;
    return 0;
}

static int
minisocket_process_syn(network_interrupt_arg_t *intrpt, minisocket_t local)
{
    if (LISTEN == local->state) {
        minisocket_server_init(intrpt, local);
        semaphore_V(local->synchonize);
    }
    return INTERRUPT_PROCESSED;
}

static int
minisocket_process_synack(network_interrupt_arg_t *intrpt, minisocket_t local)
{
    mini_header_reliable_t header = (mini_header_reliable_t) intrpt->buffer;
    int seq = unpack_unsigned_int(header->seq_number);
    int ack = unpack_unsigned_int(header->ack_number);

    if (minisocket_validate(intrpt, local) == -1)
        return INTERRUPT_PROCESSED;
    /* Remote acknowleges local sent SYN, and sends SYN back. */
    if (SYNSENT == local->state && local->seq == ack && local->ack + 1 == seq) {
        minisocket_retry_cancel(local);
        /* Acknowlege SYNACK */
        local->ack = seq;
        minisocket_acknowlege(local);
        local->state = ESTABLISHED;
    }
    return INTERRUPT_PROCESSED;
}

static int
minisocket_process_ack(network_interrupt_arg_t *intrpt, minisocket_t local)
{
    mini_header_reliable_t header = (mini_header_reliable_t) intrpt->buffer;
    int seq = unpack_unsigned_int(header->seq_number);
    int ack = unpack_unsigned_int(header->ack_number);

    if (minisocket_validate(intrpt, local) == -1)
        return INTERRUPT_PROCESSED;
    /* Disable retransmission if send is acknowleged */
    if (local->alarm > -1 && local->seq == ack) {
        if ((ESTABLISHED == local->state) || (SYNRECEIVED == local->state))
            minisocket_retry_cancel(local);
        if (SYNRECEIVED == local->state)
            local->state = ESTABLISHED;
    }

    /* Queue data */
    if (ESTABLISHED == local->state
            && local->ack + win_size >= seq && local->ack < seq) {
        if (queue_wrap_enqueue(local->data, intrpt) != 0)
            return INTERRUPT_PROCESSED;
        /* Signal waiting thread */
        if (queue_length(local->data) == 1)
            semaphore_V(local->receive);
        /* Acknowlege data received */
        local->ack = seq;
        minisocket_acknowlege(local);

        return INTERRUPT_STORED;
    }

    return INTERRUPT_PROCESSED;
}

static int
minisocket_process_fin(network_interrupt_arg_t *intrpt, minisocket_t local)
{
    return INTERRUPT_PROCESSED;
}
