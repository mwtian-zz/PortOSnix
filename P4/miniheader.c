#include "miniheader.h"

void pack_unsigned_int(char* buf, unsigned int val)
{
	unsigned char* ubuf = buf;
	ubuf[0] = (val>>24) & 0xff;
	ubuf[1] = (val>>16) & 0xff;
	ubuf[2] = (val>>8) & 0xff;
	ubuf[3] = val & 0xff;
}

unsigned int unpack_unsigned_int(char *buf)
{
	unsigned char* ubuf = buf;
	return (unsigned int) (ubuf[0]<<24) | (ubuf[1]<<16) | (ubuf[2]<<8) | ubuf[3];
}

void pack_unsigned_short(char* buf, unsigned short val)
{
	unsigned char* ubuf = buf;
	ubuf[0] = (val>>8) & 0xff;
	ubuf[1] = val & 0xff;
}

unsigned short unpack_unsigned_short(char* buf)
{
	unsigned char* ubuf = buf;
	return (unsigned short) (ubuf[0]<<8) | ubuf[1];
}

void pack_address(char* buf, network_address_t address)
{
	unsigned int* addr_ptr = (unsigned int*) address;
	pack_unsigned_int(buf, addr_ptr[0]);
	pack_unsigned_int(buf+sizeof(unsigned int), addr_ptr[1]);
}

void unpack_address(char* buf, network_address_t address)
{
	unsigned int* addr_ptr = (unsigned int*) address;
	addr_ptr[0] = unpack_unsigned_int(buf);
	addr_ptr[1] = unpack_unsigned_int(buf+sizeof(unsigned int));
}
