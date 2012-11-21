#ifndef __MINIHEADER_H__
#define __MINIHEADER_H__
/*
 *	Definitions for the network header format.
 */
#include "network.h"

#define MINIMSG_HDRSIZE (sizeof(struct mini_header))
#define MINISOCKET_HDRSIZE (sizeof(struct mini_header_reliable))


/* protocol types */
enum { PROTOCOL_MINIDATAGRAM = 1, PROTOCOL_MINISTREAM };

/* message types for minisockets */
enum { MSG_SYN = 1, MSG_SYNACK, MSG_ACK, MSG_FIN };

/* header definition for unreliable packets */
typedef struct mini_header
{
	char protocol;

	char source_address[8];
	char source_port[2];

	char destination_address[8];
	char destination_port[2];

} *mini_header_t;

/* header definition for reliable packets, note the overlap with minimsg */
typedef struct mini_header_reliable
{
	char protocol;

	char source_address[8];
	char source_port[2];

	char destination_address[8];
	char destination_port[2];

	char message_type;
	char seq_number[4];
	char ack_number[4];

} *mini_header_reliable_t;

/* packs a native unsigned short into 2 bytes in network byte order */
void pack_unsigned_short(char *buf, unsigned short val);

/* packs a native unsigned integer into 4 bytes in network byte order */
void pack_unsigned_int(char *buf, unsigned int val);

/* packs an opaque network address into 8 bytes in network byte order */
void pack_address(char *buf, network_address_t address);

/* unpacks a native unsigned integer from 2 bytes in network byte order */
unsigned short unpack_unsigned_short(char *buf);

/* unpacks a native unsigned integer from 4 bytes in network byte order */
unsigned int unpack_unsigned_int(char *buf);

/* unpacks a network address from 8 bytes in network byte order */
void unpack_address(char* buf, network_address_t address);

#endif /*__MINIHEADER_H__*/
