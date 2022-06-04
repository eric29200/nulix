#ifndef _UDP_H_
#define _UDP_H_

#include <net/sk_buff.h>

/*
 * UDP header.
 */
struct udp_header_t {
	uint16_t	src_port;
	uint16_t	dst_port;
	uint16_t	len;
	uint16_t	chksum;
};

void udp_receive(struct sk_buff_t *skb);

#endif
