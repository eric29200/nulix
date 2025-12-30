#ifndef _TCP_H_
#define _TCP_H_

#include <net/sk_buff.h>
#include <net/inet/ip.h>
#include <net/inet/net.h>
#include <stddef.h>

#define TCP_ESTABLISHED		1
#define TCP_CLOSE		7
#define TCP_LISTEN		10

#define TCPCB_FLAG_FIN		0x01
#define TCPCB_FLAG_SYN		0x02
#define TCPCB_FLAG_RST		0x04
#define TCPCB_FLAG_PSH		0x08
#define TCPCB_FLAG_ACK		0x10
#define TCPCB_FLAG_URG		0x20
#define TCPCB_FLAG_ECE		0x40
#define TCPCB_FLAG_CWR		0x80

/*
 * TCP header.
 */
struct tcp_header {
	uint16_t	src_port;
	uint16_t	dst_port;
	uint32_t	seq;
	uint32_t	ack_seq;
	uint16_t	res1:4;
	uint16_t	doff:4;
	uint16_t	fin:1;
	uint16_t	syn:1;
	uint16_t	rst:1;
	uint16_t	psh:1;
	uint16_t	ack:1;
	uint16_t	urg:1;
	uint16_t	res2:2;
	uint16_t	window;
	uint16_t	chksum;
	uint16_t	urg_ptr;
};

/*
 * TCP check header.
 */
struct tcp_check_header {
	uint32_t	src_address;
	uint32_t	dst_address;
	uint8_t		zero;
	uint8_t		protocol;
	uint16_t	len;
};

void tcp_receive(struct sk_buff *skb);

/*
 * Get TCP options length.
 */
static inline uint16_t tcp_option_length(struct sk_buff *skb)
{
	return skb->h.tcp_header->doff * 4 - sizeof(struct tcp_header);
}

/*
 * Get TCP data length.
 */
static inline uint16_t tcp_data_length(struct sk_buff *skb)
{
	return ntohs(skb->nh.ip_header->length) - (skb->nh.ip_header->ihl + skb->h.tcp_header->doff) * 4;
}

/*
 * Get TCP data.
 */
static inline void *tcp_data(struct sk_buff *skb)
{
	return (void *) skb->h.tcp_header + sizeof(struct tcp_header) + tcp_option_length(skb);
}

#endif
