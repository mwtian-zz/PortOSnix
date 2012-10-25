/*
 *	Implementation of minisockets.
 */

#include "defs.h"

#include "alarm.h"
#include "minisocket.h"
#include "minisocket_private.h"
#include "network.h"
#include "network_msg.h"
#include "queue.h"
#include "synch.h"
#include "miniheader.h"

static int minisocket_transmit(minisocket_t socket, char msg_type, char *buf,
                               int len);

static minisocket_t minisocket[MINISOCKET_MAX_NUM - MINISOCKET_MIN_NUM + 1];
static int socket_count = 0;
static int retry_delay[MINISOCKET_MAX_TRY - 1];
static int win_size = 1;
static network_address_t hostaddr;
static semaphore_t port_count_mutex = NULL;
static semaphore_t port_array_mutex = NULL;

/* Initializes the minisocket layer. */
void minisocket_initialize()
{
	network_get_my_address(hostaddr);
	if ((port_count_mutex = semaphore_create()) != NULL) {
		semaphore_initialize(port_count_mutex, 1);
	}
	if ((port_array_mutex = semaphore_create()) != NULL) {
		semaphore_initialize(port_array_mutex, 1);
	}
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
	network_interrupt_arg_t *packet;
	mini_header_reliable_t header;
	interrupt_level_t oldlevel;

	/* Port out of range */
	if (port < MINISOCKET_MIN_NUM || port > MINISOCKET_MAX_NUM) {
		*error = SOCKET_PORTOUTOFBOUND;
		return NULL;
	}

	semaphore_P(port_array_mutex);
	/* Port already in use */
	if (minisocket[port] != NULL) {
		*error = SOCKET_PORTINUSE;
		semaphore_V(port_array_mutex);
		return NULL;
	}

	/* Create the socket */
	minisocket[port] = malloc(sizeof(struct minisocket));
	semaphore_V(port_array_mutex);

	/* Assume out of memory if malloc returns NULL */
	if (minisocket[port] == NULL) {
		*error = SOCKET_OUTOFMEMORY;
		return NULL;
	}

	semaphore_P(port_count_mutex);
	socket_count++;
	semaphore_V(port_count_mutex);

	minisocket[port]->receive = semaphore_create();
	if (minisocket[port]->receive == NULL) {
		free(minisocket[port]);
		*error = SOCKET_OUTOFMEMORY; /* Assume out of memory? */
		minisocket[port] = NULL;
		semaphore_P(port_count_mutex);
		socket_count--;
		semaphore_V(port_count_mutex);
		return NULL;
	}
	semaphore_initialize(minisocket[port]->receive, 0);

	minisocket[port]->data = queue_new();
	if (minisocket[port]->data == NULL) {
		semaphore_destroy(minisocket[port]->receive);
		free(minisocket[port]);
		*error = SOCKET_OUTOFMEMORY; /* Assume out of memory? */
		minisocket[port] = NULL;
		semaphore_P(port_count_mutex);
		socket_count--;
		semaphore_V(port_count_mutex);
		return NULL;
	}

	minisocket[port]->socket_mutex = semaphore_create();
	if (minisocket[port]->socket_mutex == NULL) {
		semaphore_destroy(minisocket[port]->receive);
		queue_free(minisocket[port]->data);
		free(minisocket[port]);
		*error = SOCKET_OUTOFMEMORY; /* Assume out of memory? */
		minisocket[port] = NULL;
		semaphore_P(port_count_mutex);
		socket_count--;
		semaphore_V(port_count_mutex);
		return NULL;
	}
	semaphore_initialize(minisocket[port]->socket_mutex, 1);
	minisocket[port]->local_port_num = port;

	do {
		minisocket[port]->state = LISTEN;

		/* Wait for sync from client*/
		oldlevel = set_interrupt_level(DISABLED);
		semaphore_P(minisocket[port]->receive);
		/* TO BE DONE */
		/* dequeue from message queue
		 * and the packet is put into packet
		 */
		 set_interrupt_level(oldlevel);

		 header = (mini_header_reliable_t) packet->buffer;
		 minisocket[port]->remote_port_num = unpack_unsigned_short(header->source_port);
		 unpack_address(header->source_address, minisocket[port]->addr);
		 minisocket[port]->state = SYNRECEIVED;

		 /* Now try to send SYNACK and wait for ack*/

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
    return NULL;
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
    return 0;
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
    return 0;
}

/* Close a connection. If minisocket_close is issued, any send or receive should
 * fail.  As soon as the other side knows about the close, it should fail any
 * send or receive in progress. The minisocket is destroyed by minisocket_close
 * function.  The function should never fail.
 */
void
minisocket_close(minisocket_t socket)
{

}

static int
minisocket_transmit(minisocket_t socket, char msg_type, minimsg_t msg, int len)
{
	return -1;
}

int
minisocket_enqueue(network_interrupt_arg_t *intrpt)
{
    struct msg_node *mnode;
    mini_header_reliable_t header = (mini_header_reliable_t) intrpt->buffer;
    int local_num = unpack_unsigned_short(header->destination_port);
    int remote_num = unpack_unsigned_short(header->source_port);
    int type = header->message_type;
    /* Sanity checks kept at minimum. */
    if (local_num > MINISOCKET_MAX_NUM || local_num < MINISOCKET_MIN_NUM
            || NULL == minisocket[local_num]) {
        free(intrpt);
        return -1;
    }
    /* Disable alarm if necessary */
    if (minisocket[local_num]->seq == ack)
        if ((MSG_ACK == type && ESTABLISHED == minisocket[local_num]->state)
                || (MSG_SYNACK == type && LISTEN == minisocket[local_num]->state))
            deregister_alarm(minisocket[local_num]->alarm);

    mnode = malloc(sizeof(struct msg_node));
    if (mnode == NULL) {
        free(intrpt);
        return -1;
    }
    mnode->intrpt = intrpt;

    if (queue_append(minisocket[local_num]->data, mnode) != 0) {
        free(mnode);
        free(intrpt);
        return -1;
    }
    semaphore_V(minisocket[local_num]->receive);
    return 0;
}

/*
 * Get the next available socket, return -1 if none available
 */
static int
minisocket_get_socket() {
	int num = -1, i;

	semaphore_P(port_array_mutex);
	if (socket_count >= MINISOCKET_PORT_NUM) {
		semaphore_V(port_array_mutex);
		return num;
	}

	for (i = MINISOCKET_MIN_NUM; i <= MINISOCKET_MAX_NUM; i++) {
		if (minisocket[i] == NULL) {
			num = i;
			minisocket[i] = (minisocket_t)-1;
			semaphore_P(port_count_mutex);
			socket_count++;
			semaphore_V(port_count_mutex);
			break;
		}
	}
	semaphore_V(port_array_mutex);
	return num;
}

/* seq, ack and message_type may get from socket */
static void
minisocket_packhdr(mini_header_reliable_t header, minisocket_t socket,
                   char message_type, int seq, int ack) {
	header->protocol = PROTOCOL_MINISTREAM;
	pack_address(header->source_address, hostaddr);
	pack_unsigned_short(header->source_port, socket->local_port_num);
	pack_address(header->destination_address, socket->addr);
	pack_unsigned_short(header->destination_port, socket->remote_port_num);
	header->message_type = message_type;
	pack_unsigned_int(header->seq_number, seq);
	pack_unsigned_int(header->ack_number, ack);
}
