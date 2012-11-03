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
minisocket_initialize_socket(int port, minisocket_error *error);
static void
minisocket_destroy(minisocket_t *p_socket);
static int
minisocket_get_state(minisocket_t socket);
static void
minisocket_set_state(minisocket_t socket, int state);
static int
minisocket_process_packet(network_interrupt_arg_t *intrp);
static void
minisocket_signal_busy(network_interrupt_arg_t *intrpt);

static minisocket_t minisocket[MINISOCKET_PORT_NUM];
static int socket_count = 0;
static int retry_delay[MINISOCKET_MAX_TRY];
static network_address_t hostaddr;
static semaphore_t port_count_mutex = NULL;
static semaphore_t client_port_mutex = NULL;
static semaphore_t server_port_mutex = NULL;
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
    network_get_my_address(hostaddr);
    
    retry_delay[0] = MINISOCKET_INITIAL_TIMEOUT;
    for (i = 1; i < MINISOCKET_MAX_TRY; ++i)
        retry_delay[i] = 2 * retry_delay[i - 1];
    packet_buffer = queue_new();
    closing_sockets = queue_new();
    
    if ((port_count_mutex = semaphore_create()) != NULL) {
        semaphore_initialize(port_count_mutex, 1);
    }
    if ((client_port_mutex = semaphore_create()) != NULL) {
        semaphore_initialize(client_port_mutex, 1);
    }
    if ((server_port_mutex = semaphore_create()) != NULL) {
        semaphore_initialize(server_port_mutex, 1);
    }
    if ((control_sem = semaphore_create()) != NULL) {
        semaphore_initialize(control_sem, 0);
    }
    if ((cleanup_sem = semaphore_create()) != NULL) {
        semaphore_initialize(cleanup_sem, 0);
    }
    if ((cleanup_queue_mutex = semaphore_create()) != NULL) {
        semaphore_initialize(cleanup_queue_mutex, 1);
    }
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
    if (port < MINISOCKET_MIN_SERVER || port > MINISOCKET_MAX_SERVER) {
        *error = SOCKET_PORTOUTOFBOUND;
        return NULL;
    }

    semaphore_P(server_port_mutex);
    /* Port already in use */
    if (minisocket[port] != NULL) {
        *error = SOCKET_PORTINUSE;
        semaphore_V(server_port_mutex);
        return NULL;
    }

    /* Create the socket */
    minisocket[port] = malloc(sizeof(struct minisocket));
    semaphore_V(server_port_mutex);

    /* Assume out of memory if malloc returns NULL */
    if (NULL == minisocket[port]) {
        *error = SOCKET_OUTOFMEMORY;
        return NULL;
    }

    /* Initialize socket with port number */
    if (minisocket_initialize_socket(port, error) == -1) {
        return NULL;
    }

    do {
        minisocket_set_state(minisocket[port], LISTEN);
        /* Wait for SYN from client */
        semaphore_P(minisocket[port]->synchonize);
        /* Woke up by minisocket_process_syn */
        if (minisocket_get_state(minisocket[port]) == SYNRECEIVED)
            minisocket_transmit(minisocket[port], MSG_SYNACK, NULL, 0);
        /* If SYNACK is received, state becomes ESTABLISHED in control thread */
    } while (minisocket_get_state(minisocket[port]) != ESTABLISHED);

    return minisocket[port];
}

/* Called by minisocket_process_syn to initialize server with client info */
static int
minisocket_server_init(network_interrupt_arg_t *intrpt, minisocket_t local)
{
    mini_header_reliable_t header = (mini_header_reliable_t) intrpt->buffer;
    local->remote_port_num = unpack_unsigned_short(header->source_port);
    unpack_address(header->source_address, local->remote_addr);
    unpack_address(header->destination_address, local->local_addr);
    local->ack = unpack_unsigned_int(header->seq_number);
    local->seq = 0;
    minisocket_set_state(local, SYNRECEIVED);
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
    int source_port_num;
    if (error != NULL) {
        *error = SOCKET_NOERROR;
    } else {
        return NULL;
    }
    /* Get a free source port number */
    source_port_num = minisocket_get_free_socket();
    /* Run out of free ports */
    if (source_port_num == -1 || source_port_num < MINISOCKET_MIN_CLIENT
            || source_port_num > MINISOCKET_MAX_CLIENT) {
        *error = SOCKET_NOMOREPORTS;
        return NULL;
    }
    minisocket[source_port_num] = malloc(sizeof(struct minisocket));
    if (minisocket[source_port_num] == NULL) {
        *error = SOCKET_OUTOFMEMORY;
        return NULL;
    }

    /* Initialize socket */
    if (minisocket_initialize_socket(source_port_num, error) == -1) {
        return NULL;
    }
    network_address_copy(hostaddr, minisocket[source_port_num]->local_addr);
    minisocket[source_port_num]->state = SYNSENT;
    network_address_copy(addr, minisocket[source_port_num]->remote_addr);
    minisocket[source_port_num]->remote_port_num = port;

    /* Send SYN to server */
    if (minisocket_transmit(minisocket[source_port_num], MSG_SYN, NULL, 0) == -1) {
        if (FINRECEIVED == minisocket[source_port_num])
            *error = SOCKET_BUSY;
        else
            *error = SOCKET_NOSERVER;
        /* Connection to server fails, destroy socket */
        minisocket_destroy(&minisocket[source_port_num]);
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
			|| NULL = socket->receive_count_mutex) {
        minisocket[port] = NULL;
        *error = SOCKET_OUTOFMEMORY; /* Assume out of memory? */
        minisocket_destroy(&socket);
        return -1;
    }
    semaphore_initialize(socket->send_mutex, 1);
    semaphore_initialize(socket->data_mutex, 1);
    semaphore_initialize(socket->state_mutex, 1);
    semaphore_initialize(socket->synchonize, 0);
    semaphore_initialize(socket->retry, 0);
    semaphore_initialize(socket->receive, 0);
    semaphore_initialize(socket->seq_mutex, 0);
	semaphore_initialize(socket->receive_count_mutex);
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

    semaphore_P(client_port_mutex);
    if (socket_count >= MINISOCKET_CLIENT_NUM) {
        semaphore_V(client_port_mutex);
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
    semaphore_V(client_port_mutex);
    return num;
}

static void
minisocket_destroy(minisocket_t *p_socket)
{
    minisocket_t socket = *p_socket;
	int i;
    if (socket != NULL) {
		semaphore_P(socket->receive_count_mutex);
		for (i = 0; i < socket->receive_count; i++) {
			semaphore_V(socket->receive);
		}
		semaphore_V(socket->receive_count_mutex);
        minisocket[socket->local_port_num] = NULL;
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
		if (socket == NULL) {
			*error = SOCKET_SENDERROR;
			semaphore_V(socket->send_mutex);
			return -1;
		}
        state = minisocket_get_state(socket);
        if (state != ESTABLISHED) {
            *error = SOCKET_SENDERROR;
            semaphore_V(socket->send_mutex);
            return -1;
        }

        if (to_sent > MINISOCKET_MAX_MSG_SIZE) {
            sent = minisocket_transmit(socket, MSG_ACK, msg + total_sent, MINISOCKET_MAX_MSG_SIZE);
        } else {
            sent = minisocket_transmit(socket, MSG_ACK, msg + total_sent, to_sent);
        }
        if (sent == -1) {
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
        if (socket == NULL) {
            *error = SOCKET_RECEIVEERROR;
            return -1;
        }
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
            memcpy(msg + stored_len, header + 1, received);
            free(intrpt);
            stored_len += received;
        } else {
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
    state = minisocket_get_state(socket);
    if (state != ESTABLISHED) {
        return;
    }

    semaphore_P(socket->send_mutex);
    minisocket_set_state(socket, FINSENT);
    minisocket_transmit(socket, MSG_FIN, NULL, 0);
    minisocket_set_state(socket, CLOSED);
    semaphore_V(socket->send_mutex);

    semaphore_P(cleanup_queue_mutex);
    queue_wrap_enqueue(closing_sockets, socket);
    semaphore_V(cleanup_queue_mutex);
    semaphore_V(cleanup_sem);
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
        network_send_pkt(socket->remote_addr, MINISOCKET_HDRSIZE,
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
    pack_address(header->source_address, socket->local_addr);
    pack_unsigned_short(header->source_port, socket->local_port_num);
    pack_address(header->destination_address, socket->remote_addr);
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
    network_send_pkt(local->remote_addr, MINISOCKET_HDRSIZE, (char*)&header, 0, NULL);
}

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

    network_send_pkt(remote_addr, MINISOCKET_HDRSIZE, (char*)&header, 0, NULL);
}

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
printf("To process packet.\n");
        minisocket_process_packet(intrpt);
    }
    return 0;
}

static int
minisocket_cleanup(int *arg)
{
    interrupt_level_t oldlevel;
    minisocket_t socket;

    while (1) {
        oldlevel = set_interrupt_level(DISABLED);
        semaphore_P(cleanup_sem);
        set_interrupt_level(oldlevel);
        semaphore_P(cleanup_queue_mutex);
        queue_wrap_dequeue(closing_sockets, (void**)&socket);
        semaphore_V(cleanup_queue_mutex);
        minisocket_set_state(socket, CLOSED);
        minisocket_destroy(&minisocket[socket->local_port_num]);
    }
    return 0;
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
    if (local_num > MINISOCKET_MAX_CLIENT || local_num < MINISOCKET_MIN_SERVER
            || NULL == minisocket[local_num]) {
        network_printaddr(hostaddr);
        network_printaddr(local_addr);
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
    queue_wrap_enqueue(packet_buffer, intrpt);
    semaphore_V(control_sem);
    return 0;
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
    if (MINISOCKET_DEBUG == 1)
        printf("SYN received.\n");
    
    semaphore_P(local->state_mutex);
    if (LISTEN == local->state) {
        local->state = SYNRECEIVED;
        semaphore_V(local->state_mutex);
        minisocket_server_init(intrpt, local);
        semaphore_V(local->synchonize);
    } else {
        semaphore_V(local->state_mutex);
        minisocket_signal_busy(intrpt);
    }
    return INTERRUPT_PROCESSED;
}

static int
minisocket_process_synack(network_interrupt_arg_t *intrpt, minisocket_t local)
{
    mini_header_reliable_t header = (mini_header_reliable_t) intrpt->buffer;
    int seq = unpack_unsigned_int(header->seq_number);
    int ack = unpack_unsigned_int(header->ack_number);
    if (MINISOCKET_DEBUG == 1)
        printf("SYNACK received.\n");

    if (minisocket_validate_source(intrpt, local) == -1)
        return INTERRUPT_PROCESSED;
    /* Remote acknowleges local sent SYN, and sends SYN back. */
    if (SYNSENT == local->state && local->seq == ack) {
        minisocket_retry_cancel(local);
        /* Acknowlege SYNACK */
        local->ack = seq;
        minisocket_acknowlege(local);
        minisocket_set_state(local, ESTABLISHED);
    }
    return INTERRUPT_PROCESSED;
}

static int
minisocket_process_ack(network_interrupt_arg_t *intrpt, minisocket_t local)
{
    mini_header_reliable_t header = (mini_header_reliable_t) intrpt->buffer;
    int seq = unpack_unsigned_int(header->seq_number);
    int ack = unpack_unsigned_int(header->ack_number);
    int state;

    if (MINISOCKET_DEBUG == 1)
        printf("ACK received.\n");

    if (minisocket_validate_source(intrpt, local) == -1)
        return INTERRUPT_PROCESSED;

    state = minisocket_get_state(local);

    /* Disable retransmission if send is acknowleged */
    if (local->alarm > -1 && local->seq == ack) {
        if (ESTABLISHED == state || SYNRECEIVED == state || FINSENT == state) {
            minisocket_retry_cancel(local);
        }
        if (SYNRECEIVED == state)
            minisocket_set_state(local, ESTABLISHED);
    }

    /* Queue data */
    if (ESTABLISHED == state && local->ack + 1 == seq) {
        if (queue_wrap_enqueue(local->data, intrpt) != 0)
            return INTERRUPT_PROCESSED;
        /* Signal waiting thread */
        if (queue_length(local->data) == 1)
            semaphore_V(local->receive);
        /* Acknowlege data received */
        local->ack++;
        minisocket_acknowlege(local);

        return INTERRUPT_STORED;
    }

    return INTERRUPT_PROCESSED;
}

static int
minisocket_process_fin(network_interrupt_arg_t *intrpt, minisocket_t local)
{
    mini_header_reliable_t header = (mini_header_reliable_t) intrpt->buffer;
    int seq = unpack_unsigned_int(header->seq_number);
    int state = minisocket_get_state(local);

    if (minisocket_validate_source(intrpt, local) == -1)
        return INTERRUPT_PROCESSED;

    if (SYNSENT == local->state) {
        local->state = FINRECEIVED;
        return INTERRUPT_PROCESSED;
    }

    if (local->ack + 1 == seq) {
        if (state == ESTABLISHED || state == FINSENT) {
            local->ack++;
            minisocket_acknowlege(local);
        }
        if (state == ESTABLISHED) {
            minisocket_set_state(local, FINRECEIVED);
            register_alarm(MINISOCKET_FIN_TIMEOUT, semaphore_Signal, cleanup_sem);
            queue_wrap_enqueue(closing_sockets, local);
        }
    }

    if (state == FINRECEIVED && local->ack == seq) {
        minisocket_acknowlege(local);
    }

    return INTERRUPT_PROCESSED;
}
