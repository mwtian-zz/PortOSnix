/*
 *	Implementation of minisockets.
 */

#include "defs.h"

#include "alarm.h"
#include "miniheader.h"
#include "miniroute.h"
#include "minisocket.h"
#include "minisocket_private.h"
#include "minithread.h"
#include "network.h"
#include "queue_wrap.h"
#include "queue.h"
#include "synch.h"

/* File scope function, explained before each implementation. */
static int
minisocket_server_init_from_intrpt(network_interrupt_arg_t *intrpt,
                                   minisocket_t local);
static int
minisocket_client_init_from_input(network_address_t addr, int port,
                                  minisocket_t socket);
static int
minisocket_transmit(minisocket_t socket, char msg_type, char *buf, int len);
static void
minisocket_acknowledge(minisocket_t socket);
static void
minisocket_retry_wait(minisocket_t socket, int delay);
static void
minisocket_retry_wakeup(void *socket);
static void
minisocket_retry_cancel(minisocket_t socket, minisocket_alarm_status sig);
static void
minisocket_receive_unblock(minisocket_t socket);
static void
minisocket_packhdr(mini_header_reliable_t header, minisocket_t socket,
                   char message_type);
static int
minisocket_validate_source(network_interrupt_arg_t *intrpt, minisocket_t local);
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
minisocket_common_init(int port, minisocket_error *error);
static void
minisocket_destroy(minisocket_t *p_socket);
static void
minisocket_cleanup_prepare(minisocket_t socket);
static void
minisocket_cleanup_enqueue(minisocket_t socket);
static int
minisocket_get_state(minisocket_t socket);
static void
minisocket_set_state(minisocket_t socket, int state);
static int
minisocket_process_packet(network_interrupt_arg_t *intrp);
static void
minisocket_signal_busy(network_interrupt_arg_t *intrpt);

/* File scope variables */
static minisocket_t minisocket[MINISOCKET_PORT_NUM];
static int socket_count = 0;
static int retry_delay[MINISOCKET_MAX_TRY];
static semaphore_t port_count_mutex = NULL;
static semaphore_t port_array_mutex = NULL;
static semaphore_t control_sem = NULL;
static semaphore_t cleanup_sem = NULL;
static semaphore_t cleanup_queue_mutex = NULL;

static queue_t packet_buffer;
static queue_t closing_sockets;
static int minisocket_control(int *arg);
static int minisocket_cleanup(int *arg);

/* Initializes the minisocket layer. */
void
minisocket_initialize()
{
    int i;

    retry_delay[0] = MINISOCKET_INITIAL_TIMEOUT;
    for (i = 1; i < MINISOCKET_MAX_TRY; ++i) {
        retry_delay[i] = 2 * retry_delay[i - 1];
    }
    packet_buffer = queue_new();
    closing_sockets = queue_new();

    port_count_mutex = semaphore_create();
    port_array_mutex = semaphore_create();
    control_sem = semaphore_create();
    cleanup_sem = semaphore_create();
    cleanup_queue_mutex = semaphore_create();

    if (NULL == port_count_mutex || NULL == port_array_mutex
            || NULL == control_sem || NULL == cleanup_sem
            || NULL == cleanup_queue_mutex) {
        return;
    }

    semaphore_initialize(port_count_mutex, 1);
    semaphore_initialize(port_array_mutex, 1);
    semaphore_initialize(control_sem, 0);
    semaphore_initialize(cleanup_sem, 0);
    semaphore_initialize(cleanup_queue_mutex, 1);

    minithread_fork(minisocket_control, NULL);
    minithread_fork(minisocket_cleanup, NULL);
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
    if (error == NULL) {
        return NULL;
    } else {
        *error = SOCKET_NOERROR;
    }
    /* Port out of range */
    if (port < MINISOCKET_MIN_PORT || port > MINISOCKET_MAX_PORT) {
        *error = SOCKET_PORTOUTOFBOUND;
        return NULL;
    }

    semaphore_P(port_array_mutex);
    if (minisocket[port] != NULL) {
        /* Port already in use. Return error. */
        semaphore_V(port_array_mutex);
        *error = SOCKET_PORTINUSE;
        return NULL;
    } else {
        /* Port not in use. Create the socket */
        minisocket[port] = malloc(sizeof(struct minisocket));
    }
    semaphore_V(port_array_mutex);

    if (NULL == minisocket[port]) {
        *error = SOCKET_OUTOFMEMORY;
        return NULL;
    }

    if (minisocket_common_init(port, error) == -1) {
        return NULL;
    }

    semaphore_P(minisocket[port]->state_mutex);
    do {
        minisocket[port]->state = LISTEN;
        semaphore_V(minisocket[port]->state_mutex);
        /* Wait for SYN from client */
        semaphore_P(minisocket[port]->synchonize);
        /* Woke up by minisocket_process_syn */
        if (minisocket_get_state(minisocket[port]) == SYNRECEIVED)
            minisocket_transmit(minisocket[port], MSG_SYNACK, NULL, 0);
        /*
         * If a SYNACK is received, state would be set to ESTABLISHED
         * by the control thread.
         */
        semaphore_P(minisocket[port]->state_mutex);
    } while (SYNRECEIVED== minisocket[port]->state);
    semaphore_V(minisocket[port]->state_mutex);

    semaphore_P(port_count_mutex);
    socket_count++;
    semaphore_V(port_count_mutex);

    return minisocket[port];
}

/* Called by minisocket_process_syn to initialize server with client info */
static int
minisocket_server_init_from_intrpt(network_interrupt_arg_t *intrpt, minisocket_t local)
{
    mini_header_reliable_t header = (mini_header_reliable_t) intrpt->buffer;
    local->remote_port_num = unpack_unsigned_short(header->source_port);
    unpack_address(header->source_address, local->remote_addr);
    unpack_address(header->destination_address, local->local_addr);
    local->ack = unpack_unsigned_int(header->seq_number);
    return 0;
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
    int port_num;
    minisocket_t socket;

    if (error != NULL) {
        *error = SOCKET_NOERROR;
    } else {
        return NULL;
    }

    semaphore_P(port_array_mutex);
    port_num = minisocket_get_free_socket();
    if (port_num == -1 || port_num < MINISOCKET_MIN_PORT
            || port_num > MINISOCKET_MAX_PORT) {
        *error = SOCKET_NOMOREPORTS;
        return NULL;
    }
    minisocket[port_num] = malloc(sizeof(struct minisocket));
    semaphore_V(port_array_mutex);

    if (minisocket[port_num] == NULL) {
        *error = SOCKET_OUTOFMEMORY;
        return NULL;
    }

    /* Initialize socket */
    socket = minisocket[port_num];
    if (minisocket_common_init(port_num, error) == -1) {
        return NULL;
    }
    semaphore_P(socket->state_mutex);
    minisocket_client_init_from_input(addr, port, socket);
    socket->state = SYNSENT;
    semaphore_V(minisocket[port_num]->state_mutex);

    /* Send SYN to server */
    if (minisocket_transmit(socket, MSG_SYN, NULL, 0) < 0) {
        if (TIMEWAIT == minisocket_get_state(socket))
            *error = SOCKET_BUSY;
        else
            *error = SOCKET_NOSERVER;
        /* Connection to server fails, destroy socket */
        minisocket_cleanup_enqueue(minisocket[port_num]);
        return NULL;
    }

    semaphore_P(port_count_mutex);
    socket_count++;
    semaphore_V(port_count_mutex);

    return socket;
}

static int
minisocket_client_init_from_input(network_address_t addr, int port,
                                  minisocket_t socket)
{
    network_address_copy(hostaddr, socket->local_addr);
    network_address_copy(addr, socket->remote_addr);
    socket->remote_port_num = port;
    return 0;
}

/* Get the next available socket, return -1 if none available */
static int
minisocket_get_free_socket()
{
    int num = -1;
    int i;

    if (socket_count >= MINISOCKET_PORT_NUM) {
        semaphore_V(port_array_mutex);
        return -1;
    }
    for (i = MINISOCKET_MAX_PORT; i >= MINISOCKET_MIN_PORT; i--) {
        if (minisocket[i] == NULL) {
            num = i;
            break;
        }
    }

    return num;
}

/* Initialize socket with port number, return -1 on failure, 0 on success*/
static int
minisocket_common_init(int port, minisocket_error *error)
{
    minisocket_t socket = minisocket[port];
    if (NULL == socket) {
        *error = SOCKET_OUTOFMEMORY;
        return -1;
    }
    socket->state = CLOSED;
    socket->local_port_num = port;
    socket->seq = 0;
    socket->ack = 0;
    socket->receive_count = 0;
    socket->send_mutex = semaphore_create();
    socket->data_mutex = semaphore_create();
    socket->state_mutex = semaphore_create();
    socket->seq_mutex = semaphore_create();
    socket->synchonize = semaphore_create();
    socket->retry = semaphore_create();
    socket->receive = semaphore_create();
    socket->receive_count_mutex = semaphore_create();
    socket->data = queue_new();
    if (NULL == socket->send_mutex || NULL == socket->synchonize
            || NULL == socket->retry || NULL == socket->receive
            || NULL == socket->data || NULL == socket->data_mutex
            || NULL == socket->state_mutex || NULL == socket->seq_mutex
            || NULL == socket->receive_count_mutex) {
        minisocket[port] = NULL;
        *error = SOCKET_OUTOFMEMORY; /* Assume out of memory? */
        minisocket_cleanup_enqueue(socket);
        return -1;
    }
    semaphore_initialize(socket->send_mutex, 1);
    semaphore_initialize(socket->data_mutex, 1);
    semaphore_initialize(socket->state_mutex, 1);
    semaphore_initialize(socket->synchonize, 0);
    semaphore_initialize(socket->retry, 0);
    semaphore_initialize(socket->receive, 0);
    semaphore_initialize(socket->seq_mutex, 0);
    semaphore_initialize(socket->receive_count_mutex, 1);
    return 0;
}

/* Release memory of closed ports */
static void
minisocket_destroy(minisocket_t *p_socket)
{
    minisocket_t socket = *p_socket;
    network_interrupt_arg_t *intrpt;
    if (socket != NULL) {
        semaphore_P(port_array_mutex);
        minisocket[socket->local_port_num] = NULL;
        semaphore_V(port_array_mutex);
        while (queue_wrap_dequeue(socket->data, (void**)&intrpt) == 0)
            free(intrpt);
        queue_free(socket->data);
        semaphore_destroy(socket->receive);
        semaphore_destroy(socket->send_mutex);
        semaphore_destroy(socket->data_mutex);
        semaphore_destroy(socket->state_mutex);
        semaphore_destroy(socket->seq_mutex);
        semaphore_destroy(socket->synchonize);
        semaphore_destroy(socket->retry);
        free(socket);

        semaphore_P(port_count_mutex);
        socket_count--;
        semaphore_V(port_count_mutex);
#if MINISOCKET_DEBUG == 1
        printf("socket count: %d\n", socket_count);
#endif
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
    int state;

    if (error == NULL || socket == NULL) {
        *error = SOCKET_SENDERROR;
        return -1;
    }

    semaphore_P(socket->send_mutex);
    while (to_sent > 0) {
        state = minisocket_get_state(socket);
        if (state != ESTABLISHED) {
            *error = SOCKET_SENDERROR;
            semaphore_V(socket->send_mutex);
            return -1;
        }

        if (to_sent > MINISOCKET_MAX_MSG_SIZE) {
            sent = minisocket_transmit(socket, MSG_ACK, msg + total_sent,
                                       MINISOCKET_MAX_MSG_SIZE);
        } else {
            sent = minisocket_transmit(socket, MSG_ACK, msg + total_sent, to_sent);
        }

        if (sent < 0) {
            *error = SOCKET_SENDERROR;
            semaphore_V(socket->send_mutex);
            return -1;
        }
        total_sent += sent;
        to_sent -= sent;
    }
    semaphore_V(socket->send_mutex);
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
    char *origin;
    char *dest;
    int val;
    int state;

    network_interrupt_arg_t *intrpt;
    mini_header_reliable_t header;

    if (error == NULL || socket == NULL) {
        *error = SOCKET_RECEIVEERROR;
        return -1;
    }

    semaphore_P(socket->receive_count_mutex);
    state = minisocket_get_state(socket);
    if (state != ESTABLISHED) {
        *error = SOCKET_RECEIVEERROR;
        semaphore_V(socket->receive_count_mutex);
        return -1;
    }
    socket->receive_count++;
    semaphore_V(socket->receive_count_mutex);

    semaphore_P(socket->receive);

    semaphore_P(socket->receive_count_mutex);
    socket->receive_count--;
    semaphore_V(socket->receive_count_mutex);

    while (1) {
        state = minisocket_get_state(socket);
        if (state != ESTABLISHED) {
            *error = SOCKET_RECEIVEERROR;
            return -1;
        }
        semaphore_P(socket->data_mutex);
        val = queue_wrap_dequeue(socket->data, (void**) &intrpt);
        semaphore_V(socket->data_mutex);
        if (val != 0) {
            break;
        }

        header = (mini_header_reliable_t) intrpt->buffer;
        received = intrpt->size - MINISOCKET_HDRSIZE;
        if (stored_len + received <= max_len) {
            /* Copy all data */
            memcpy(msg + stored_len, header + 1, received);
            free(intrpt);
            stored_len += received;
        } else {
            /* Copy data that can fit into the remaining buffer */
            memcpy(msg + stored_len, header + 1, max_len - stored_len);
            origin = (char*)(header + 1) + max_len - stored_len;
            dest = (char*)(header + 1);
            while ( origin < (char*)header + intrpt->size) {
                *(dest++) = *(origin++);
            }
            intrpt->size -= (max_len - stored_len);
            stored_len = max_len;
            queue_wrap_prepend(socket->data, intrpt);
            semaphore_V(socket->receive);
            break;
        }
    }
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
    int state;
    if (socket == NULL) {
        return;
    }
    semaphore_P(socket->state_mutex);
    state = socket->state;
    if (state != ESTABLISHED) {
        semaphore_V(socket->state_mutex);
        return;
    }
    socket->state = LASTACK;
    semaphore_V(socket->state_mutex);

    minisocket_cleanup_prepare(socket);

    semaphore_P(socket->send_mutex);
    minisocket_transmit(socket, MSG_FIN, NULL, 0);
    semaphore_V(socket->send_mutex);

    semaphore_V(cleanup_sem);
    minisocket_cleanup_enqueue(socket);
}

/* Enqueue the socket to the closing queue */
static void
minisocket_cleanup_enqueue(minisocket_t socket)
{
    semaphore_P(cleanup_queue_mutex);
    queue_wrap_enqueue(closing_sockets, socket);
    semaphore_V(cleanup_queue_mutex);
}

/* Make the threads in retransmission and waiting for receive fail */
static void
minisocket_cleanup_prepare(minisocket_t socket)
{
    /* if (ALARM_SUCCESS != socket->alarm)
        minisocket_retry_cancel(socket, ALARM_CANCELED);
    */
    minisocket_receive_unblock(socket);
}

/* Unblock threads waiting on receive semaphore */
static void
minisocket_receive_unblock(minisocket_t socket)
{
    int i;
    semaphore_P(socket->receive_count_mutex);
    for (i = 0; i < socket->receive_count; i++) {
        semaphore_V(socket->receive);
    }
    semaphore_V(socket->receive_count_mutex);
}

/* Return -1 on failure, length of transmitted on success */
static int
minisocket_transmit(minisocket_t socket, char msg_type, minimsg_t msg, int len)
{
    int i;
    struct mini_header_reliable header;
    network_address_t remote;

    semaphore_P(socket->state_mutex);
    ++socket->seq;
    network_address_copy(socket->remote_addr, remote);
    minisocket_packhdr(&header, socket, msg_type);
    semaphore_V(socket->state_mutex);

    for (i = 0; i < MINISOCKET_MAX_TRY; ++i) {
        miniroute_send_pkt(remote, MINISOCKET_HDRSIZE, (char*)&header, len, msg);
        minisocket_retry_wait(socket, retry_delay[i]);
//printf("Wait %d, delay %d, current tick %ld.\n", i, retry_delay[i], ticks);
        /* Alarm disabled because ACK received. */
        if (ALARM_SUCCESS == socket->alarm) {
#if MINISOCKET_DEBUG == 1
            printf("Message type %d. Success at try %d\n", msg_type, i);
#endif
            return len;
        }
        /* Alarm disabled because socket is closing. */
        else if (ALARM_CANCELED == socket->alarm)
            break;
    }
#if MINISOCKET_DEBUG == 1
    printf("Message type %d. Failure at try %d\n", msg_type, i);
#endif
    socket->alarm = ALARM_SUCCESS;
    return -1;
}

static void
minisocket_packhdr(mini_header_reliable_t header, minisocket_t socket,
                   char message_type)
{
    header->protocol = PROTOCOL_MINISTREAM;
    pack_address(header->source_address, socket->local_addr);
    pack_unsigned_short(header->source_port, socket->local_port_num);
    pack_address(header->destination_address, socket->remote_addr);
    pack_unsigned_short(header->destination_port, socket->remote_port_num);
    header->message_type = message_type;
    pack_unsigned_int(header->seq_number, socket->seq);
    pack_unsigned_int(header->ack_number, socket->ack);
}

/* Register alarm for retransmission after delay */
static void
minisocket_retry_wait(minisocket_t socket, int delay)
{
    socket->alarm = register_alarm(delay, minisocket_retry_wakeup, socket);
//printf("Registered alarm: %d\n", socket->alarm);
    semaphore_P(socket->retry);
}

/* Wake the thread waiting on retry on 'socket' */
static void
minisocket_retry_wakeup(void *socket)
{
    minisocket_t skt = socket;
    skt->alarm = ALARM_WAKEUP;
    semaphore_V(skt->retry);
}

/* Cancel retransmission */
static void
minisocket_retry_cancel(minisocket_t socket, minisocket_alarm_status sig)
{
    if (socket->alarm > -1) {
        deregister_alarm(socket->alarm);
//printf("Deregistered alarm: %d\n", socket->alarm);
        socket->alarm = sig;
        semaphore_V(socket->retry);
    }
}

/* Acknowledge package received */
static void
minisocket_acknowledge(minisocket_t local)
{
    struct mini_header_reliable header;
    minisocket_packhdr(&header, local, MSG_ACK);
#if MINISOCKET_DEBUG == 1
    printf("Acknowledgement sent.\n");
#endif
    miniroute_send_pkt(local->remote_addr, MINISOCKET_HDRSIZE, (char*)&header,
                       0, NULL);
}

/* Server already in connection. Reply to SYN with FIN. */
static void
minisocket_signal_busy(network_interrupt_arg_t *intrpt)
{
    struct mini_header_reliable header;
    mini_header_t recved_hdr = (mini_header_t) intrpt->buffer;
    network_address_t local_addr;
    network_address_t remote_addr;
    int local_num;
    int remote_num;
    unpack_address(recved_hdr->destination_address, local_addr);
    unpack_address(recved_hdr->source_address, remote_addr);
    local_num = unpack_unsigned_short(recved_hdr->destination_port);
    remote_num = unpack_unsigned_short(recved_hdr->source_port);

    header.protocol = PROTOCOL_MINISTREAM;
    pack_address(header.source_address, local_addr);
    pack_unsigned_short(header.source_port, local_num);
    pack_address(header.destination_address, remote_addr);
    pack_unsigned_short(header.destination_port, remote_num);
    header.message_type = MSG_FIN;
    pack_unsigned_int(header.seq_number, 1);
    pack_unsigned_int(header.ack_number, 1);

    miniroute_send_pkt(remote_addr, MINISOCKET_HDRSIZE, (char*)&header, 0, NULL);
}

/* Independent thread handling control messages and sorting data packets. */
static int
minisocket_control(int *arg)
{
    interrupt_level_t oldlevel;
    network_interrupt_arg_t *intrpt;

    while (1) {
        oldlevel = set_interrupt_level(DISABLED);
        semaphore_P(control_sem);
        queue_wrap_dequeue(packet_buffer, (void**)&intrpt);
        set_interrupt_level(oldlevel);
        minisocket_process_packet(intrpt);
    }
    return 0;
}

/* Independent thread cleaning up closed sockets. */
static int
minisocket_cleanup(int *arg)
{
    minisocket_t socket;
    int i, qlen;
    while (1) {

        semaphore_P(cleanup_sem);
        semaphore_V(cleanup_sem);

        semaphore_P(cleanup_queue_mutex);
        qlen = queue_length(closing_sockets);
        semaphore_V(cleanup_queue_mutex);

        for (i = 0; i < qlen; ++i) {
            semaphore_P(cleanup_sem);

            semaphore_P(cleanup_queue_mutex);
            queue_wrap_dequeue(closing_sockets, (void**)&socket);
            semaphore_V(cleanup_queue_mutex);

            if (NULL != socket) {
                semaphore_P(socket->receive_count_mutex);

                if (socket->receive_count > 0) {
                    semaphore_V(socket->receive_count_mutex);
                    semaphore_V(cleanup_sem);
                    minisocket_receive_unblock(socket);
                    semaphore_P(cleanup_queue_mutex);
                    queue_wrap_enqueue(closing_sockets, socket);
                    semaphore_V(cleanup_queue_mutex);
                } else {
                    semaphore_V(socket->receive_count_mutex);
                    minisocket_set_state(socket, CLOSED);
                    minisocket_destroy(&minisocket[socket->local_port_num]);
                }
            }
        }
    }

    return 0;
}

/* Call by minisocket_control to sort packets. */
int
minisocket_process_packet(network_interrupt_arg_t *intrpt)
{
    minisocket_interrupt_status intrpt_status;
    mini_header_reliable_t header = (mini_header_reliable_t) intrpt->buffer;
    minisocket_t local;
    network_address_t local_addr;
    int local_num = unpack_unsigned_short(header->destination_port);
    int type = header->message_type;
    /* Sanity checks kept at minimum. */
    unpack_address(header->destination_address, local_addr);
    if (local_num > MINISOCKET_MAX_PORT || local_num < MINISOCKET_MIN_PORT
            || NULL == minisocket[local_num]) {
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

int
minisocket_process(network_interrupt_arg_t *intrpt)
{
    if (queue_wrap_enqueue(packet_buffer, intrpt) == 0) {
        semaphore_V(control_sem);
        return 0;
    }

    return -1;
}

static int
minisocket_validate_source(network_interrupt_arg_t *intrpt, minisocket_t local)
{
    mini_header_reliable_t header = (mini_header_reliable_t) intrpt->buffer;
    network_address_t remote_addr;
    int remote_num = unpack_unsigned_short(header->source_port);
    unpack_address(header->source_address, remote_addr);
    if (network_address_same(local->remote_addr, remote_addr) != 1) {
        return -1;
    }
    if (local->remote_port_num != remote_num)
        return -1;
    return 0;
}

static int
minisocket_process_syn(network_interrupt_arg_t *intrpt, minisocket_t local)
{
    mini_header_reliable_t header = (mini_header_reliable_t) intrpt->buffer;
    int seq = unpack_unsigned_int(header->seq_number);

#if (MINISOCKET_DEBUG == 1)
    printf("SYN received. State: %d.\n", local->state);
#endif
    semaphore_P(local->state_mutex);
    switch (local->state) {
    case LISTEN:
        local->state = SYNRECEIVED;
        minisocket_server_init_from_intrpt(intrpt, local);
        semaphore_V(local->synchonize);
        break;
    case SYNSENT:
        if (minisocket_validate_source(intrpt, local) == -1) {
            semaphore_V(local->state_mutex);
            return INTERRUPT_PROCESSED;
        } else {
            local->ack = seq;
            minisocket_acknowledge(local);
        }
        break;
    default:
        if (minisocket_validate_source(intrpt, local) == -1) {
            minisocket_signal_busy(intrpt);
        } else {
            minisocket_acknowledge(local);
        }
    }
    semaphore_V(local->state_mutex);

    return INTERRUPT_PROCESSED;
}

static int
minisocket_process_synack(network_interrupt_arg_t *intrpt, minisocket_t local)
{
#if (MINISOCKET_DEBUG == 1)
    printf("SYNACK received. State: %d.\n", local->state);
#endif
    minisocket_process_syn(intrpt, local);
    return minisocket_process_ack(intrpt, local);
}

static int
minisocket_process_ack(network_interrupt_arg_t *intrpt, minisocket_t local)
{
    mini_header_reliable_t header = (mini_header_reliable_t) intrpt->buffer;
    int seq = unpack_unsigned_int(header->seq_number);
    int ack = unpack_unsigned_int(header->ack_number);
    minisocket_interrupt_status intrpt_status = INTERRUPT_PROCESSED;

#if (MINISOCKET_DEBUG == 1)
    printf("ACK received. State: %d.\n", local->state);
#endif

    if (minisocket_validate_source(intrpt, local) == -1)
        return INTERRUPT_PROCESSED;

    semaphore_P(local->state_mutex);
    /* The packet acknowledges previously sent packet. */
    if (local->seq == ack) {
        if (local->alarm > -1) {
            minisocket_retry_cancel(local, ALARM_SUCCESS);
        }
        switch (local->state) {
        case ESTABLISHED:
            break;

        case SYNSENT:
            /* The remote-sent SYN(ACK) has been received. */
            if (local->ack == seq)
                local->state = ESTABLISHED;
            break;

        case SYNRECEIVED:
            /* When local-sent SYN(ACK) is acknowledged */
            local->state = ESTABLISHED;
            break;

        case LASTACK:
            local->state = CLOSED;
            break;

        default:
            ;
        }
    }
    /* Notify remote to stop retransmitting packets that have been received */
    if (intrpt->size > MINISOCKET_HDRSIZE && local->ack == seq)
        minisocket_acknowledge(local);
    /* Store the packet that has not been seen before */
    if (ESTABLISHED == local->state && local->ack + 1 == seq) {
        /* Enqueue data and signal the thread waiting for data */
        semaphore_P(local->data_mutex);
        if (queue_wrap_enqueue(local->data, intrpt) == 0) {
            if (queue_length(local->data) == 1) {
                semaphore_V(local->receive);
                /* Acknowlege data received */
                local->ack++;
                minisocket_acknowledge(local);
                semaphore_V(local->state_mutex);
                intrpt_status = INTERRUPT_STORED;
            }
        }
        semaphore_V(local->data_mutex);
    }
    semaphore_V(local->state_mutex);

    return intrpt_status;
}

static int
minisocket_process_fin(network_interrupt_arg_t *intrpt, minisocket_t local)
{
    mini_header_reliable_t header = (mini_header_reliable_t) intrpt->buffer;
    int seq = unpack_unsigned_int(header->seq_number);

#if (MINISOCKET_DEBUG == 1)
    printf("FIN received. State: %d.\n", local->state);
#endif
    if (minisocket_validate_source(intrpt, local) == -1)
        return INTERRUPT_PROCESSED;

    semaphore_P(local->state_mutex);
    switch (local->state) {
    case SYNSENT:
        local->state = TIMEWAIT;
        break;

    case ESTABLISHED:
        if (local->ack + 1 == seq) {
            local->state = TIMEWAIT;
            local->ack++;
            minisocket_acknowledge(local);
            minisocket_cleanup_prepare(local);
            register_alarm(MINISOCKET_FIN_TIMEOUT, semaphore_Signal, cleanup_sem);
            minisocket_cleanup_enqueue(local);
        }
        break;

    case LASTACK:
        if (local->ack + 1 == seq) {
            local->ack++;
            minisocket_acknowledge(local);
        }
        break;

    case TIMEWAIT:
        if (local->ack == seq) {
            minisocket_acknowledge(local);
        }
        break;

    default:
        ;
    }
    semaphore_V(local->state_mutex);

    return INTERRUPT_PROCESSED;
}

static int
minisocket_get_state(minisocket_t socket)
{
    int state;
    semaphore_P(socket->state_mutex);
    state = socket->state;
    semaphore_V(socket->state_mutex);
    return state;
}

static void
minisocket_set_state(minisocket_t socket, int state)
{
    semaphore_P(socket->state_mutex);
    socket->state = state;
    semaphore_V(socket->state_mutex);
}
