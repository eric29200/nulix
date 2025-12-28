#include <net/sock.h>
#include <net/inet/tcp.h>
#include <net/inet/net.h>
#include <net/inet/ethernet.h>
#include <proc/sched.h>
#include <stderr.h>
#include <stdio.h>
#include <math.h>
#include <uio.h>

/*
 * Compute TCP checksum.
 */
static uint16_t tcp_checksum(struct tcp_header *tcp_header, uint8_t *src_address, uint8_t *dst_address, size_t len)
{
	uint16_t *chunk, ret;
	uint32_t chksum;
	size_t size;

	/* compute size = tcp header + len */
	size = sizeof(struct tcp_header) + len;

	/* build TCP check header */
	struct tcp_check_header tcp_check_header = {
		.src_address		= inet_iton(src_address),
		.dst_address		= inet_iton(dst_address),
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
 * Build a TCP header.
 */
static void tcp_build_header(struct tcp_header *tcp_header, uint16_t src_port, uint16_t dst_port,
			     uint32_t seq, uint32_t ack_seq, uint16_t window, uint16_t flags)
{
	tcp_header->src_port = htons(src_port);
	tcp_header->dst_port = htons(dst_port);
	tcp_header->seq = htonl(seq);
	tcp_header->ack_seq = htonl(ack_seq);
	tcp_header->res1 = 0;
	tcp_header->doff = sizeof(struct tcp_header) / 4;
	tcp_header->fin = (flags & TCPCB_FLAG_FIN) != 0;
	tcp_header->syn = (flags & TCPCB_FLAG_SYN) != 0;
	tcp_header->rst = (flags & TCPCB_FLAG_RST) != 0;
	tcp_header->psh = (flags & TCPCB_FLAG_PSH) != 0;
	tcp_header->ack = (flags & TCPCB_FLAG_ACK) != 0;
	tcp_header->urg = (flags & TCPCB_FLAG_URG) != 0;
	tcp_header->res2 = 0;
	tcp_header->window = htons(window);
	tcp_header->chksum = 0;
	tcp_header->urg_ptr = 0;
}

/*
 * Receive/decode a TCP packet.
 */
void tcp_receive(struct sk_buff *skb)
{
	skb->h.tcp_header = (struct tcp_header *) skb->data;
	skb_pull(skb, sizeof(struct tcp_header));
}

/*
 * Create a TCP message.
 */
static struct sk_buff *tcp_create_skb(struct sock *sk, uint16_t flags, void *msg, size_t len)
{
	struct sk_buff *skb;
	uint8_t dest_ip[4];
	void *buf;

	/* get destination IP */
	inet_ntoi(sk->protinfo.af_inet.dst_sin.sin_addr, dest_ip);

	/* allocate a socket buffer */
	skb = skb_alloc(sizeof(struct ethernet_header) + sizeof(struct ip_header) + sizeof(struct tcp_header) + len);
	if (!skb)
		return NULL;

	/* build ethernet header */
	skb->eth_header = (struct ethernet_header *) skb_put(skb, sizeof(struct ethernet_header));
	ethernet_build_header(skb->eth_header, sk->protinfo.af_inet.dev->mac_addr, NULL, ETHERNET_TYPE_IP);

	/* build ip header */
	skb->nh.ip_header = (struct ip_header *) skb_put(skb, sizeof(struct ip_header));
	ip_build_header(skb->nh.ip_header, 0, sizeof(struct ip_header) + sizeof(struct tcp_header) + len, 0,
			IPV4_DEFAULT_TTL, IP_PROTO_TCP, sk->protinfo.af_inet.dev->ip_addr, dest_ip);

	/* build tcp header */
	skb->h.tcp_header = (struct tcp_header *) skb_put(skb, sizeof(struct tcp_header));
	tcp_build_header(skb->h.tcp_header, ntohs(sk->protinfo.af_inet.src_sin.sin_port), ntohs(sk->protinfo.af_inet.dst_sin.sin_port),
			 sk->protinfo.af_inet.seq_no, sk->protinfo.af_inet.ack_no, ETHERNET_MAX_MTU, flags);

	/* copy message */
	buf = skb_put(skb, len);
	memcpy(buf, msg, len);

	/* compute tcp checksum */
	skb->h.tcp_header->chksum = tcp_checksum(skb->h.tcp_header, sk->protinfo.af_inet.dev->ip_addr, dest_ip, len);

	/* update sequence */
	if (len)
		sk->protinfo.af_inet.seq_no += len;
	else if ((flags & TCPCB_FLAG_SYN) || (flags & TCPCB_FLAG_FIN))
		sk->protinfo.af_inet.seq_no += 1;

	return skb;
}

/*
 * Reply a ACK message.
 */
static int tcp_reply_ack(struct sock *sk, struct sk_buff *skb, uint16_t flags)
{
	struct sk_buff *skb_ack;
	uint8_t dest_ip[4];
	int len;

	/* get destination IP */
	inet_ntoi(inet_iton(skb->nh.ip_header->src_addr), dest_ip);

	/* allocate a socket buffer */
	skb_ack = skb_alloc(sizeof(struct ethernet_header) + sizeof(struct ip_header) + sizeof(struct tcp_header));
	if (!skb_ack)
		return -ENOMEM;

	/* build ethernet header */
	skb_ack->eth_header = (struct ethernet_header *) skb_put(skb_ack, sizeof(struct ethernet_header));
	ethernet_build_header(skb_ack->eth_header, sk->protinfo.af_inet.dev->mac_addr, NULL, ETHERNET_TYPE_IP);

	/* build ip header */
	skb_ack->nh.ip_header = (struct ip_header *) skb_put(skb_ack, sizeof(struct ip_header));
	ip_build_header(skb_ack->nh.ip_header, 0, sizeof(struct ip_header) + sizeof(struct tcp_header), 0,
			IPV4_DEFAULT_TTL, IP_PROTO_TCP, sk->protinfo.af_inet.dev->ip_addr, dest_ip);

	/* compute ack number */
	len = tcp_data_length(skb);
	sk->protinfo.af_inet.ack_no = ntohl(skb->h.tcp_header->seq) + len;
	if (!len)
		sk->protinfo.af_inet.ack_no += 1;

	/* build tcp header */
	skb_ack->h.tcp_header = (struct tcp_header *) skb_put(skb_ack, sizeof(struct tcp_header));
	tcp_build_header(skb_ack->h.tcp_header, ntohs(sk->protinfo.af_inet.src_sin.sin_port), ntohs(skb->h.tcp_header->src_port),
			 sk->protinfo.af_inet.seq_no, sk->protinfo.af_inet.ack_no, ETHERNET_MAX_MTU, flags);

	/* compute tcp checksum */
	skb_ack->h.tcp_header->chksum = tcp_checksum(skb_ack->h.tcp_header, sk->protinfo.af_inet.dev->ip_addr, dest_ip, 0);

	/* transmit ACK message */
	net_transmit(sk->protinfo.af_inet.dev, skb_ack);

	/* update sequence */
	if (flags & TCPCB_FLAG_SYN || flags & TCPCB_FLAG_FIN)
		sk->protinfo.af_inet.seq_no += 1;

	return 0;
}

/*
 * Reset a TCP connection.
 */
static int tcp_reset(struct sock *sk, uint32_t seq_no)
{
	struct sk_buff *skb;

	/* set sequence number */
	sk->protinfo.af_inet.seq_no = ntohl(seq_no);
	sk->protinfo.af_inet.ack_no = 0;

	/* create a RST packet */
	skb = tcp_create_skb(sk, TCPCB_FLAG_RST, NULL, 0);
	if (!skb)
		return -EINVAL;

	/* transmit SYN message */
	net_transmit(sk->protinfo.af_inet.dev, skb);

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
	if (sk->protinfo.af_inet.src_sin.sin_port != skb->h.tcp_header->dst_port)
		return -EINVAL;

	/* check source */
	if ((sk->sock->state == SS_CONNECTED || sk->sock->state == SS_CONNECTING)
	    && sk->protinfo.af_inet.dst_sin.sin_port != skb->h.tcp_header->src_port)
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
	switch (sk->sock->state) {
		case SS_LISTENING:
			/* clone socket buffer */
			skb_new = skb_clone(skb);
			if (!skb_new)
				return -ENOMEM;

			/* add buffer to socket */
			if (data_len > 0 || skb->h.tcp_header->syn || skb->h.tcp_header->fin)
				skb_queue_tail(&sk->receive_queue, skb_new);

			break;
		case SS_CONNECTING:
			/* find SYN | ACK message */
			if (skb->h.tcp_header->syn && skb->h.tcp_header->ack) {
				sk->sock->state = SS_CONNECTED;
				goto out;
			}

			/* else reset TCP connection and release socket */
			tcp_reset(sk, skb->h.tcp_header->ack_seq);
			sk->sock->state = SS_DEAD;

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
					sk->sock->state = SS_DISCONNECTING;

			break;
		default:
			break;
	}

out:
	/* wake up eventual processes */
	wake_up(&sk->sock->wait);

	return 0;
}

/*
 * Receive a TCP message.
 */
static int tcp_recvmsg(struct sock *sk, struct msghdr *msg, size_t size)
{
	size_t len, n, i, count = 0;
	struct sockaddr_in *sin;
	struct sk_buff *skb;
	void *buf;

	/* unused size */
	UNUSED(size);

	/* sleep until we receive a packet */
	for (;;) {
		/* dead socket */
		if (sk->sock->state == SS_DEAD)
			return -ENOTCONN;

		/* signal received : restart system call */
		if (signal_pending(current_task))
			return -ERESTARTSYS;

		/* message received : break */
		if (!skb_queue_empty(&sk->receive_queue))
			break;

		/* disconnected : break */
		if (sk->sock->state == SS_DISCONNECTING)
			return 0;

		/* non blocking */
		if (msg->msg_flags & MSG_DONTWAIT)
			return -EAGAIN;

		/* sleep */
		sleep_on(&sk->sock->wait);
	}

	/* get first message */
	skb = skb_dequeue(&sk->receive_queue);

	/* get IP header */
	skb->nh.ip_header = (struct ip_header *) (skb->head + sizeof(struct ethernet_header));

	/* get TCP header */
	skb->h.tcp_header = (struct tcp_header *) (skb->head + sizeof(struct ethernet_header) + sizeof(struct ip_header));

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
	sin = (struct sockaddr_in *) msg->msg_name;
	if (sin) {
		sin->sin_family = AF_INET;
		sin->sin_port = skb->h.tcp_header->src_port;
		sin->sin_addr = inet_iton(skb->nh.ip_header->src_addr);
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
	struct sk_buff *skb;
	size_t i;
	int len;

	/* unused size */
	UNUSED(size);

	/* sleep until connected */
	for (;;) {
		/* dead socket */
		if (sk->sock->state == SS_DEAD)
			return -ENOTCONN;

		/* signal received : restart system call */
		if (signal_pending(current_task))
			return -ERESTARTSYS;

		/* connected : break */
		if (sk->sock->state == SS_CONNECTED)
			break;

		/* non blocking */
		if (msg->msg_flags & MSG_DONTWAIT)
			return -EAGAIN;

		/* sleep */
		sleep_on(&sk->sock->wait);
	}

	for (i = 0, len = 0; i < msg->msg_iovlen; i++) {
		/* create socket buffer */
		skb = tcp_create_skb(sk, TCPCB_FLAG_ACK, msg->msg_iov->iov_base, msg->msg_iov->iov_len);
		if (!skb)
			return -EINVAL;

		/* transmit message */
		net_transmit(sk->protinfo.af_inet.dev, skb);

		len += msg->msg_iov->iov_len;
	}

	return len;
}

/*
 * Create a TCP connection.
 */
static int tcp_connect(struct sock *sk)
{
	struct sk_buff *skb;

	/* generate sequence */
	sk->protinfo.af_inet.seq_no = ntohl(rand());
	sk->protinfo.af_inet.ack_no = 0;

	/* create SYN message */
	skb = tcp_create_skb(sk, TCPCB_FLAG_SYN, NULL, 0);
	if (!skb)
		return -EINVAL;

	/* transmit SYN message */
	net_transmit(sk->protinfo.af_inet.dev, skb);

	/* set socket state */
	sk->sock->state = SS_CONNECTING;

	return 0;
}

/*
 * Find a SYN message.
 */
static struct sk_buff *tcp_find_established(struct sock *sk)
{
	struct sk_buff *skb;

	/* peek first message */
	skb = skb_peek(&sk->receive_queue);
	if (!skb)
		return NULL;

	/* find a SYN message */
	do {
		/* get IP header */
		skb->nh.ip_header = (struct ip_header *) (skb->head + sizeof(struct ethernet_header));

		/* get TCP header */
		skb->h.tcp_header = (struct tcp_header *) (skb->head + sizeof(struct ethernet_header) + sizeof(struct ip_header));

		/* SYN message */
		if (!skb->h.tcp_header->syn)
			return skb;

		skb = skb->next;
	} while (skb != (struct sk_buff *) &sk->receive_queue);

	return NULL;
}

/*
 * Accept a TCP connection.
 */
static int tcp_accept(struct sock *sk, struct sock *sk_new)
{
	struct sk_buff *skb;

	for (;;) {
		/* wait for a SYN message */
		skb = tcp_find_established(sk);
		if (!skb) {
			/* signal received : restart system call */
			if (signal_pending(current_task))
				return -ERESTARTSYS;

			/* disconnected : break */
			if (sk->sock->state == SS_DISCONNECTING)
				return 0;

			/* sleep */
			sleep_on(&sk->sock->wait);
			continue;
		}

		/* set new socket */
		sk_new->sock->state = SS_CONNECTED;
		memcpy(&sk_new->protinfo.af_inet.src_sin, &sk->protinfo.af_inet.src_sin, sizeof(struct sockaddr));
		sk_new->protinfo.af_inet.dst_sin.sin_family = AF_INET;
		sk_new->protinfo.af_inet.dst_sin.sin_port = skb->h.tcp_header->src_port;
		sk_new->protinfo.af_inet.dst_sin.sin_addr = inet_iton(skb->nh.ip_header->src_addr);
		sk_new->protinfo.af_inet.seq_no = ntohl(1);
		sk_new->protinfo.af_inet.ack_no = ntohl(skb->h.tcp_header->seq) + 1;

		/* free socket buffer */
		skb_unlink(skb);
		skb_free(skb);

		/* move buffers to new socket */
		while ((skb = skb_dequeue(&sk->receive_queue)) != NULL) {
			/* decode socket buffer */
			skb->nh.ip_header = (struct ip_header *) (skb->head + sizeof(struct ethernet_header));
			skb->h.tcp_header = (struct tcp_header *) (skb->head + sizeof(struct ethernet_header) + sizeof(struct ip_header));

			/* move socket buffer */
			if (sk_new->protinfo.af_inet.dst_sin.sin_port == skb->h.tcp_header->src_port
				&& sk_new->protinfo.af_inet.dst_sin.sin_addr == inet_iton(skb->nh.ip_header->src_addr))
				skb_queue_tail(&sk_new->receive_queue, skb);
			else
				skb_free(skb);
		}

		return 0;
	}

	return 0;
}

/*
 * Close a TCP connection.
 */
static int tcp_close(struct sock *sk)
{
	struct sk_buff *skb;

	/* socket no connected : no need to send FIN message */
	if (sk->sock->state != SS_CONNECTED) {
		sk->sock->state = SS_DISCONNECTING;
		goto wait_for_ack;
	}

	/* send FIN message */
	skb = tcp_create_skb(sk, TCPCB_FLAG_FIN | TCPCB_FLAG_ACK, NULL, 0);
	if (skb)
		net_transmit(sk->protinfo.af_inet.dev, skb);

	/* wait for ACK message */
wait_for_ack:
	while (sk->sock->state != SS_DISCONNECTING) {
		/* signal received : restart system call */
		if (signal_pending(current_task))
			return -ERESTARTSYS;

		/* sleep */
		sleep_on(&sk->sock->wait);
	}

	return 0;
}

/*
 * TCP protocol.
 */
struct proto tcp_proto = {
	.handle		= tcp_handle,
	.close		= tcp_close,
	.recvmsg	= tcp_recvmsg,
	.sendmsg	= tcp_sendmsg,
	.connect	= tcp_connect,
	.accept		= tcp_accept,
};
