#ifndef _TCP_H_
#define _TCP_H_

#include <net/sk_buff.h>
#include <net/inet/ip.h>
#include <net/inet/net.h>
#include <stddef.h>

#define TCP_ESTABLISHED		1
#define TCP_SYN_SENT		2
#define TCP_SYN_RECV		3
#define TCP_FIN_WAIT1		4
#define TCP_FIN_WAIT2		5
#define TCP_TIME_WAIT		6
#define TCP_CLOSE		7
#define TCP_CLOSE_WAIT		8
#define TCP_LAST_ACK		9
#define TCP_LISTEN		10
#define TCP_CLOSING		11

#define TCPF_ESTABLISHED	(1 << TCP_ESTABLISHED)
#define TCPF_SYN_SENT		(1 << TCP_SYN_SENT)
#define TCPF_SYN_RECV		(1 << TCP_SYN_RECV)
#define TCPF_FIN_WAIT1		(1 << TCP_FIN_WAIT1)
#define TCPF_FIN_WAIT2		(1 << TCP_FIN_WAIT2)
#define TCPF_TIME_WAIT		(1 << TCP_TIME_WAIT)
#define TCPF_CLOSE		(1 << TCP_CLOSE)
#define TCPF_CLOSE_WAIT		(1 << TCP_CLOSE_WAIT)
#define TCPF_LAST_ACK		(1 << TCP_LAST_ACK)
#define TCPF_LISTEN		(1 << TCP_LISTEN)
#define TCPF_CLOSING		(1 << TCP_CLOSING)

#define MAX_SYN_SIZE		(sizeof(struct ip_header) + 40 + sizeof(struct tcp_header) + 4 + MAX_HEADER + 15)
#define MAX_FIN_SIZE		(sizeof(struct ip_header) + 40 + sizeof(struct tcp_header) + MAX_HEADER + 15)
#define MAX_ACK_SIZE		(sizeof(struct ip_header) + 40 + sizeof(struct tcp_header) + MAX_HEADER + 15)

#define TCP_TIME_CLOSE		4
#define TCP_TIME_DONE		7

#define TCP_FIN_TIMEOUT		(3 * HZ)
#define TCP_DONE_TIME		(2 * HZ)

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

/*
 * TCP packet.
 */
struct tcp_skb_cb {
	uint32_t	seq;		/* starting sequence number */
	uint32_t	end_seq;	/* SEQ + FIN + SYN + datalen */
	uint32_t	ack_seq;	/* sequence number ACK'd */
};

#define TCP_SKB_CB(skb)		((struct tcp_skb_cb *) &((skb)->cb[0]))

void tcp_receive(struct sk_buff *skb);
int tcp_rcv_state_process(struct sock *sk, struct sk_buff *skb);
int tcp_rcv_established(struct sock *sk, struct sk_buff *skb);
uint16_t tcp_checksum(struct tcp_header *tcp_header, uint32_t src_address, uint32_t dst_address, size_t len);
int tcp_send_ack(struct sock *sk, int syn, int fin);
int tcp_send_syn(struct sock *sk);
int tcp_send_fin(struct sock *sk);
int tcp_send_message(struct sock *sk, const struct msghdr *msg, size_t len);
void tcp_set_timer(struct sock *sk, int timeout, time_t expires);

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

/*
 * Is a TCP socket connected ?
 */
static inline int tcp_connected(int state)
{
	return ((1 << state) & (TCPF_ESTABLISHED | TCPF_CLOSE_WAIT | TCPF_FIN_WAIT1 | TCPF_FIN_WAIT2 | TCPF_SYN_RECV));
}

#endif
