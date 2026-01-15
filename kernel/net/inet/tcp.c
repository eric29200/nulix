#include <net/sock.h>
#include <net/inet/tcp.h>
#include <net/inet/net.h>
#include <net/inet/ethernet.h>
#include <net/inet/route.h>
#include <proc/sched.h>
#include <fcntl.h>
#include <stderr.h>
#include <stdio.h>
#include <math.h>
#include <uio.h>

/*
 * TCP fake header.
 */
struct tcp_fake_header {
	struct tcp_header	th;
	uint32_t		saddr;
	uint32_t		daddr;
	struct iovec *		iov;
};

/*
 * Compute TCP checksum.
 */
static uint16_t tcp_checksum(struct tcp_header *tcp_header, uint32_t src_address, uint32_t dst_address, size_t len)
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
 * Receive/decode a TCP packet.
 */
void tcp_receive(struct sk_buff *skb)
{
	/* decode TCP header */
	skb->h.tcp_header = (struct tcp_header *) skb->data;
	skb_pull(skb, sizeof(struct tcp_header));
}

/*
 * Copy a TCP fragment.
 */
static void tcp_getfrag(const void *ptr, char *to, size_t fraglen)
{
	struct tcp_fake_header *tfh = (struct tcp_fake_header *) ptr;
	struct tcp_header *th;

	memcpy(to, &tfh->th, sizeof(struct tcp_header));
	memcpy_fromiovec((uint8_t *) to + sizeof(struct tcp_header), tfh->iov, fraglen - sizeof(struct tcp_header));

	/* compute tcp checksum */
	th = (struct tcp_header *) to;
	th->chksum = tcp_checksum(th, tfh->saddr, tfh->daddr, fraglen - sizeof(struct tcp_header));
}

/*
 * Send a TCP message.
 */
static int tcp_send_skb(struct sock *sk, uint16_t flags, const struct msghdr *msg, size_t len)
{
	size_t tlen = len + sizeof(struct tcp_header);
	struct tcp_fake_header tfh;
	struct route *rt;
	uint32_t saddr;
	int ret;

	/* find route if needed */
	saddr = sk->saddr;
	if (!saddr) {
		rt = ip_rt_route(sk->daddr);
		if (!rt)
			return -ENETUNREACH;
		saddr = rt->rt_dev->ip_addr;
	}

	/* build tcp header */
	tfh.saddr = saddr;
	tfh.daddr = sk->daddr;
	tfh.th.src_port = sk->sport;
	tfh.th.dst_port = sk->dport;
	tfh.th.seq = htonl(sk->protinfo.af_tcp.seq_no);
	tfh.th.ack_seq = htonl(sk->protinfo.af_tcp.ack_no);
	tfh.th.res1 = 0;
	tfh.th.doff = sizeof(struct tcp_header) / 4;
	tfh.th.fin = (flags & TCPCB_FLAG_FIN) != 0;
	tfh.th.syn = (flags & TCPCB_FLAG_SYN) != 0;
	tfh.th.rst = (flags & TCPCB_FLAG_RST) != 0;
	tfh.th.psh = (flags & TCPCB_FLAG_PSH) != 0;
	tfh.th.ack = (flags & TCPCB_FLAG_ACK) != 0;
	tfh.th.urg = (flags & TCPCB_FLAG_URG) != 0;
	tfh.th.res2 = 0;
	tfh.th.window = htons(ETHERNET_MAX_MTU);
	tfh.th.chksum = 0;
	tfh.th.urg_ptr = 0;
	tfh.iov = msg->msg_iov;

	/* build and transmit ip packet */
	ret = ip_build_xmit(sk, tcp_getfrag, &tfh, tlen, sk->daddr);
	if (ret)
		return ret;

	/* update sequence */
	if (len)
		sk->protinfo.af_tcp.seq_no += len;
	else if ((flags & TCPCB_FLAG_SYN) || (flags & TCPCB_FLAG_FIN))
		sk->protinfo.af_tcp.seq_no += 1;

	return 0;
}

/*
 * Reply a ACK message.
 */
static int tcp_reply_ack(struct sock *sk, struct sk_buff *skb, uint16_t flags)
{
	struct tcp_fake_header tfh;
	uint32_t daddr, saddr;
	struct route *rt;
	int len, ret;

	/* get destination IP */
	daddr = skb->nh.ip_header->src_addr;

	/* find route if needed */
	saddr = sk->saddr;
	if (!saddr) {
		rt = ip_rt_route(daddr);
		if (!rt)
			return -ENETUNREACH;
		saddr = rt->rt_dev->ip_addr;
	}

	/* compute ack number */
	len = tcp_data_length(skb);
	sk->protinfo.af_tcp.ack_no = ntohl(skb->h.tcp_header->seq) + len;
	if (!len)
		sk->protinfo.af_tcp.ack_no += 1;

	/* build tcp header */
	tfh.saddr = saddr;
	tfh.daddr = daddr;
	tfh.th.src_port = sk->sport;
	tfh.th.dst_port = skb->h.tcp_header->src_port;
	tfh.th.seq = htonl(sk->protinfo.af_tcp.seq_no);
	tfh.th.ack_seq = htonl(sk->protinfo.af_tcp.ack_no);
	tfh.th.res1 = 0;
	tfh.th.doff = sizeof(struct tcp_header) / 4;
	tfh.th.fin = (flags & TCPCB_FLAG_FIN) != 0;
	tfh.th.syn = (flags & TCPCB_FLAG_SYN) != 0;
	tfh.th.rst = (flags & TCPCB_FLAG_RST) != 0;
	tfh.th.psh = (flags & TCPCB_FLAG_PSH) != 0;
	tfh.th.ack = (flags & TCPCB_FLAG_ACK) != 0;
	tfh.th.urg = (flags & TCPCB_FLAG_URG) != 0;
	tfh.th.res2 = 0;
	tfh.th.window = htons(ETHERNET_MAX_MTU);
	tfh.th.chksum = 0;
	tfh.th.urg_ptr = 0;
	tfh.iov = NULL;

	/* build and transmit ip packet */
	ret = ip_build_xmit(sk, tcp_getfrag, &tfh, sizeof(struct tcp_header), daddr);
	if (ret)
		return ret;

	/* update sequence */
	if (flags & TCPCB_FLAG_SYN || flags & TCPCB_FLAG_FIN)
		sk->protinfo.af_tcp.seq_no += 1;

	return 0;
}

/*
 * Reset a TCP connection.
 */
static int tcp_reset(struct sock *sk, uint32_t seq_no)
{
	struct msghdr msg = { 0 };
	int ret;

	/* set sequence number */
	sk->protinfo.af_tcp.seq_no = ntohl(seq_no);
	sk->protinfo.af_tcp.ack_no = 0;

	/* transmit a RST packet */
	ret = tcp_send_skb(sk, TCPCB_FLAG_RST, &msg, 0);
	if (ret)
		return ret;

	return 0;
}

/*
 * Handle a TCP packet.
 */
static int tcp_handle(struct sock *sk, struct sk_buff *skb)
{
	struct sk_buff *skb_new;
	uint16_t ack_flags;
	uint32_t data_len;

	/* check protocol */
	if (sk->protocol != skb->nh.ip_header->protocol)
		return -EINVAL;

	/* check destination */
	if (sk->sport != skb->h.tcp_header->dst_port)
		return -EINVAL;

	/* check source */
	if ((sk->socket->state == SS_CONNECTED || sk->socket->state == SS_CONNECTING) && sk->dport != skb->h.tcp_header->src_port)
		return -EINVAL;

	/* compute data length */
	data_len = tcp_data_length(skb);

	/* set ack flags */
	ack_flags = TCPCB_FLAG_ACK;
	if (skb->h.tcp_header->syn && !skb->h.tcp_header->ack)
		ack_flags |= TCPCB_FLAG_SYN;
	else if (skb->h.tcp_header->fin && !skb->h.tcp_header->ack)
		ack_flags |= TCPCB_FLAG_FIN;

	/* send ACK message */
	if (data_len > 0 || skb->h.tcp_header->syn || skb->h.tcp_header->fin)
		tcp_reply_ack(sk, skb, ack_flags);

	/* handle TCP packet */
	switch (sk->socket->state) {
		case SS_CONNECTING:
			/* find SYN | ACK message */
			if (skb->h.tcp_header->syn && skb->h.tcp_header->ack) {
				sk->socket->state = SS_CONNECTED;
				goto out;
			}

			/* else reset TCP connection and release socket */
			tcp_reset(sk, skb->h.tcp_header->ack_seq);
			sk->dead = 1;

			break;
		case SS_CONNECTED:
				/* add message */
				if (data_len > 0 || skb->h.tcp_header->fin) {
					/* clone socket buffer */
					skb_new = skb_clone(skb);
					if (!skb_new)
						return -ENOMEM;

					/* add buffer to socket */
					skb_queue_tail(&sk->receive_queue, skb_new);
				}

				/* FIN message : close socket */
				if (skb->h.tcp_header->fin)
					sk->socket->state = SS_DISCONNECTING;

			break;
		default:
			break;
	}

out:
	/* wake up eventual processes */
	wake_up(&sk->socket->wait);

	return 0;
}

/*
 * Receive a TCP message.
 */
static int tcp_recvmsg(struct sock *sk, struct msghdr *msg, size_t size)
{
	struct sockaddr_in *addr_in = (struct sockaddr_in *) msg->msg_name;
	size_t len, n, i, count = 0;
	struct sk_buff *skb;
	void *buf;

	/* unused size */
	UNUSED(size);

	/* sleep until we receive a packet */
	for (;;) {
		/* dead socket */
		if (sk->dead)
			return -ENOTCONN;

		/* signal received : restart system call */
		if (signal_pending(current_task))
			return -ERESTARTSYS;

		/* message received : break */
		if (!skb_queue_empty(&sk->receive_queue))
			break;

		/* disconnected : break */
		if (sk->socket->state == SS_DISCONNECTING)
			return 0;

		/* non blocking */
		if (msg->msg_flags & MSG_DONTWAIT)
			return -EAGAIN;

		/* sleep */
		sleep_on(&sk->socket->wait);
	}

	/* get first message */
	skb = skb_dequeue(&sk->receive_queue);

	/* get message */
	buf = tcp_data(skb) + sk->msg_position;
	len = tcp_data_length(skb) - sk->msg_position;

	/* copy message */
	for (i = 0; i < msg->msg_iovlen && len > 0; i++) {
		n = len > msg->msg_iov[i].iov_len ? msg->msg_iov[i].iov_len : len;
		memcpy(msg->msg_iov[i].iov_base, buf, n);
		count += n;
		len -= n;
		buf += n;
	}

	/* set source address */
	if (addr_in) {
		addr_in->sin_family = AF_INET;
		addr_in->sin_port = skb->h.tcp_header->src_port;
		addr_in->sin_addr = skb->nh.ip_header->src_addr;
	}

	/* free message or requeue it */
	if (!(msg->msg_flags & MSG_PEEK)) {
		if (len > 0) {
			sk->msg_position += count;
			skb_queue_head(&sk->receive_queue, skb);
		} else {
			sk->msg_position = 0;
			skb_free(skb);
		}
	} else {
		skb_queue_head(&sk->receive_queue, skb);
	}


	return count;
}

/*
 * Send a TCP message.
 */
static int tcp_sendmsg(struct sock *sk, const struct msghdr *msg, size_t size)
{
	int ret;

	/* sleep until connected */
	for (;;) {
		/* dead socket */
		if (sk->dead)
			return -ENOTCONN;

		/* signal received : restart system call */
		if (signal_pending(current_task))
			return -ERESTARTSYS;

		/* connected : break */
		if (sk->socket->state == SS_CONNECTED)
			break;

		/* non blocking */
		if (msg->msg_flags & MSG_DONTWAIT)
			return -EAGAIN;

		/* sleep */
		sleep_on(&sk->socket->wait);
	}

	/* send message */
	ret = tcp_send_skb(sk, TCPCB_FLAG_ACK, msg, size);
	if (ret)
		return ret;

	return size;
}

/*
 * Create a TCP connection.
 */
static int tcp_connect(struct sock *sk)
{
	struct msghdr msg = { 0 };
	int ret;

	/* generate sequence */
	sk->protinfo.af_tcp.seq_no = ntohl(rand());
	sk->protinfo.af_tcp.ack_no = 0;

	/* send SYN message */
	ret = tcp_send_skb(sk, TCPCB_FLAG_SYN, &msg, 0);
	if (ret)
		return ret;

	/* set socket state */
	sk->socket->state = SS_CONNECTING;

	return 0;
}

/*
 * Close a TCP connection.
 */
static int tcp_close(struct sock *sk)
{
	struct msghdr msg = { 0 };
	int ret;

	/* socket no connected : no need to send FIN message */
	if (sk->socket->state != SS_CONNECTED) {
		sk->socket->state = SS_DISCONNECTING;
		goto wait_for_ack;
	}

	/* send FIN message */
	ret = tcp_send_skb(sk, TCPCB_FLAG_FIN | TCPCB_FLAG_ACK, &msg, 0);
	if (ret)
		return ret;

	/* wait for ACK message */
wait_for_ack:
	while (sk->socket->state != SS_DISCONNECTING) {
		/* signal received : restart system call */
		if (signal_pending(current_task))
			return -ERESTARTSYS;

		/* sleep */
		sleep_on(&sk->socket->wait);
	}

	return 0;
}

/*
 * TCP protocol.
 */
struct proto tcp_prot = {
	.handle		= tcp_handle,
	.close		= tcp_close,
	.recvmsg	= tcp_recvmsg,
	.sendmsg	= tcp_sendmsg,
	.connect	= tcp_connect,
};
