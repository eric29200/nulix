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
 * Create a TCP packet.
 */
int tcp_send_skb(struct sock *sk, struct iovec *iov, size_t len, uint8_t flags)
{
	struct tcp_opt *tp = &sk->protinfo.af_tcp;
	struct net_device *dev;
	struct tcp_header *th;
	struct sk_buff *skb;
	int ret;

	/* allocate a packet */
	skb = sock_wmalloc(sk, MAX_TCP_HEADER_SIZE + len, 0);
	if (!skb)
		return -ENOMEM;
	skb->free = 1;

	/* build IP header */
	ret = ip_build_header(skb, sk->daddr, sizeof(struct tcp_header) + len, &dev, sk->ip_ttl);
	if (ret) {
		skb_free(skb);
		return ret;
	}

	/* build TCP header */
	skb->h.tcp_header = th = (struct tcp_header *) skb_put(skb, sizeof(struct tcp_header) + len);
	th->src_port = sk->sport;
	th->dst_port = sk->dport;
	th->seq = htonl(tp->snd_nxt);
	th->ack_seq = htonl(tp->rcv_nxt);
	th->doff = sizeof(struct tcp_header) / 4;
	th->fin = (flags & TCPCB_FLAG_FIN) != 0;
	th->syn = (flags & TCPCB_FLAG_SYN) != 0;
	th->rst = (flags & TCPCB_FLAG_RST) != 0;
	th->psh = (flags & TCPCB_FLAG_PSH) != 0;
	th->ack = (flags & TCPCB_FLAG_ACK) != 0;
	th->urg = (flags & TCPCB_FLAG_URG) != 0;
	th->window = htons(ETHERNET_MAX_MTU);

	/* copy data */
	if (len)
		memcpy_fromiovec((uint8_t *) th + sizeof(struct tcp_header), iov, len);

	/* compute checksum */
	th->chksum = tcp_checksum(th, dev->ip_addr, sk->daddr, len);

	/* transmit packet */
	dev_queue_xmit(dev, skb);

	/* update sequence number */
	if (len > 0)
		tp->snd_nxt += len;
	else if (th->syn || th->fin)
		tp->snd_nxt++;

	return 0;
}