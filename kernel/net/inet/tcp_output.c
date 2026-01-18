#include <net/inet/tcp.h>
#include <net/inet/ethernet.h>
#include <net/sock.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Compute TCP checksum.
 */
uint16_t tcp_checksum(struct tcp_header *tcp_header, uint32_t src_address, uint32_t dst_address, size_t len)
{
	uint16_t *chunk, ret;
	uint32_t chksum;
	size_t size;

	/* compute size = tcp header + len */
	size = sizeof(struct tcp_header) + len;

	/* build TCP check header */
	struct tcp_check_header tcp_check_header = {
		.src_address		= src_address,
		.dst_address		= dst_address,
		.zero			= 0,
		.protocol		= IP_PROTO_TCP,
		.len			= htons(size),
	};

	/* compute check sum on TCP check header */
	size = sizeof(struct tcp_check_header);
	for (chksum = 0, chunk = (uint16_t *) &tcp_check_header; size > 1; size -= 2)
		chksum += *chunk++;

	if (size == 1)
		chksum += *((uint8_t *) chunk);

	/* compute check sum on TCP header */
	size = sizeof(struct tcp_header) + len;
	for (chunk = (uint16_t *) tcp_header; size > 1; size -= 2)
		chksum += *chunk++;

	if (size == 1)
		chksum += *((uint8_t *) chunk);

	chksum = (chksum & 0xFFFF) + (chksum >> 16);
	chksum += (chksum >> 16);
	ret = ~chksum;

	return ret;
}

/*
 * Send a ACK message.
 */
int tcp_send_ack(struct sock *sk, int syn, int fin)
{
	struct net_device *dev;
	struct tcp_header *th;
	struct sk_buff *skb;
	int ret;

	/* allocate a packet */
	skb = sock_wmalloc(sk, MAX_ACK_SIZE, 0);
	if (!skb)
		return -ENOMEM;

	/* build IP header */
	ret = ip_build_header(skb, sk->daddr, sizeof(struct tcp_header), &dev);
	if (ret) {
		skb_free(skb);
		return ret;
	}

	/* build TCP header */
	skb->h.tcp_header = th = (struct tcp_header *) skb_put(skb, sizeof(struct tcp_header));
	th->src_port = sk->sport;
	th->dst_port = sk->dport;
	th->seq = htonl(sk->protinfo.af_tcp.seq_no);
	th->ack_seq = htonl(sk->protinfo.af_tcp.ack_no);
	th->doff = sizeof(struct tcp_header) / 4;
	th->ack = 1;
	th->syn = syn;
	th->fin = fin;
	th->window = htons(ETHERNET_MAX_MTU);
	th->chksum = tcp_checksum(th, dev->ip_addr, sk->daddr, 0);

	/* transmit packet */
	net_transmit(dev, skb);

	return 0;
}

/*
 * Send a SYN message.
 */
int tcp_send_syn(struct sock *sk)
{
	struct net_device *dev;
	struct tcp_header *th;
	struct sk_buff *skb;
	int ret;

	/* allocate a packet */
	skb = sock_wmalloc(sk, MAX_SYN_SIZE, 0);
	if (!skb)
		return -ENOMEM;

	/* build IP header */
	ret = ip_build_header(skb, sk->daddr, sizeof(struct tcp_header), &dev);
	if (ret) {
		skb_free(skb);
		return ret;
	}

	/* build TCP header */
	skb->h.tcp_header = th = (struct tcp_header *) skb_put(skb, sizeof(struct tcp_header));
	th->src_port = sk->sport;
	th->dst_port = sk->dport;
	th->seq = htonl(sk->protinfo.af_tcp.seq_no);
	th->ack_seq = htonl(sk->protinfo.af_tcp.ack_no);
	th->doff = sizeof(struct tcp_header) / 4;
	th->syn = 1;
	th->window = htons(ETHERNET_MAX_MTU);
	th->chksum = tcp_checksum(th, dev->ip_addr, sk->daddr, 0);

	/* transmit packet */
	net_transmit(dev, skb);

	/* update sequence number */
	sk->protinfo.af_tcp.seq_no++;

	return 0;
}

/*
 * Send a FIN message.
 */
int tcp_send_fin(struct sock *sk)
{
	struct net_device *dev;
	struct tcp_header *th;
	struct sk_buff *skb;
	int ret;

	/* allocate a packet */
	skb = sock_wmalloc(sk, MAX_FIN_SIZE, 0);
	if (!skb)
		return -ENOMEM;

	/* build IP header */
	ret = ip_build_header(skb, sk->daddr, sizeof(struct tcp_header), &dev);
	if (ret) {
		skb_free(skb);
		return ret;
	}

	/* build TCP header */
	skb->h.tcp_header = th = (struct tcp_header *) skb_put(skb, sizeof(struct tcp_header));
	th->src_port = sk->sport;
	th->dst_port = sk->dport;
	th->seq = htonl(sk->protinfo.af_tcp.seq_no);
	th->ack_seq = htonl(sk->protinfo.af_tcp.ack_no);
	th->doff = sizeof(struct tcp_header) / 4;
	th->fin = 1;
	th->ack = 1;
	th->window = htons(ETHERNET_MAX_MTU);
	th->chksum = tcp_checksum(th, dev->ip_addr, sk->daddr, 0);

	/* transmit packet */
	net_transmit(dev, skb);

	/* update sequence number */
	sk->protinfo.af_tcp.seq_no++;

	return 0;
}

/*
 * Send a TCP message.
 */
int tcp_send_message(struct sock *sk, const struct msghdr *msg, size_t len)
{
	struct net_device *dev;
	struct tcp_header *th;
	struct sk_buff *skb;
	int ret;

	/* allocate a packet */
	skb = sock_wmalloc(sk, MAX_HEADER + sizeof(struct ip_header) + sizeof(struct tcp_header) + len, 0);
	if (!skb)
		return -ENOMEM;

	/* build IP header */
	ret = ip_build_header(skb, sk->daddr, sizeof(struct tcp_header) + len, &dev);
	if (ret) {
		skb_free(skb);
		return ret;
	}

	/* build TCP header */
	skb->h.tcp_header = th = (struct tcp_header *) skb_put(skb, sizeof(struct tcp_header) + len);
	th->src_port = sk->sport;
	th->dst_port = sk->dport;
	th->seq = htonl(sk->protinfo.af_tcp.seq_no);
	th->ack_seq = htonl(sk->protinfo.af_tcp.ack_no);
	th->doff = sizeof(struct tcp_header) / 4;
	th->ack = 1;
	th->window = htons(ETHERNET_MAX_MTU);

	/* copy data */
	memcpy_fromiovec((uint8_t *) th + sizeof(struct tcp_header), msg->msg_iov, len);

	/* compute checksum */
	th->chksum = tcp_checksum(th, dev->ip_addr, sk->daddr, len);

	/* transmit packet */
	net_transmit(dev, skb);

	/* update sequence number */
	sk->protinfo.af_tcp.seq_no += len;

	return 0;
}