#ifndef _NET_IN_H_
#define _NET_IN_H_

#include <stddef.h>

/*
 * Inet socket address.
 */
struct sockaddr_in {
	uint16_t		sin_family;
	uint16_t		sin_port;
	uint32_t		sin_addr;
	uint8_t		 	sin_pad[8];
};

#endif