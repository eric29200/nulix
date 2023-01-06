#include <net/inet/tcp.h>
#include <net/inet/sock.h>
#include <net/inet/net.h>
#include <net/inet/ethernet.h>
#include <proc/sched.h>
#include <stderr.h>
#include <math.h>
#include <uio.h>

/*
 * Compute TCP checksum.
 */
static uint16_t tcp_checksum(struct tcp_header_t *tcp_header, uint8_t *src_address, uint8_t *dst_address, size_t len)
{
	uint16_t *chunk, ret;
	uint32_t chksum;
	size_t size;

	/* compute size = tcp header + len */
	size = sizeof(struct tcp_header_t) + len;

	/* build TCP check header */
	struct tcp_check_header_t tcp_check_header = {
		.src_address		= inet_iton(src_address),
		.dst_address		= inet_iton(dst_address),
		.zero			= 0,
		.protocol		= IP_PROTO_TCP,
		.len			= htons(size),
	};

	/* compute check sum on TCP check header */
	size = sizeof(struct tcp_check_header_t);
	for (chksum = 0, chunk = (uint16_t *) &tcp_check_header; size > 1; size -= 2)
		chksum += *chunk++;

	if (size == 1)
		chksum += *((uint8_t *) chunk);

	/* compute check sum on TCP header */
	size = sizeof(struct tcp_header_t) + len;
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
static void tcp_build_header(struct tcp_header_t *tcp_header, uint16_t src_port, uint16_t dst_port,
			     uint32_t seq, uint32_t ack_seq, uint16_t window, uint16_t flags)
{
	tcp_header->src_port = htons(src_port);
	tcp_header->dst_port = htons(dst_port);
	tcp_header->seq = htonl(seq);
	tcp_header->ack_seq = htonl(ack_seq);
	tcp_header->res1 = 0;
	tcp_header->doff = sizeof(struct tcp_header_t) / 4;
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
void tcp_receive(struct sk_buff_t *skb)
{
	skb->h.tcp_header = (struct tcp_header_t *) skb->data;
	skb_pull(skb, sizeof(struct tcp_header_t));
}

/*
 * Create a TCP message.
 */
static struct sk_buff_t *tcp_create_skb(struct sock_t *sk, uint16_t flags, void *msg, size_t len)
{
	struct sk_buff_t *skb;
	uint8_t dest_ip[4];
	void *buf;

	/* get destination IP */
	inet_ntoi(sk->dst_sin.sin_addr, dest_ip);

	/* allocate a socket buffer */
	skb = skb_alloc(sizeof(struct ethernet_header_t) + sizeof(struct ip_header_t) + sizeof(struct tcp_header_t) + len);
	if (!skb)
		return NULL;

	/* build ethernet header */
	skb->eth_header = (struct ethernet_header_t *) skb_put(skb, sizeof(struct ethernet_header_t));
	ethernet_build_header(skb->eth_header, sk->dev->mac_addr, NULL, ETHERNET_TYPE_IP);

	/* build ip header */
	skb->nh.ip_header = (struct ip_header_t *) skb_put(skb, sizeof(struct ip_header_t));
	ip_build_header(skb->nh.ip_header, 0, sizeof(struct ip_header_t) + sizeof(struct tcp_header_t) + len, 0,
			IPV4_DEFAULT_TTL, IP_PROTO_TCP, sk->dev->ip_addr, dest_ip);

	/* build tcp header */
	skb->h.tcp_header = (struct tcp_header_t *) skb_put(skb, sizeof(struct tcp_header_t));
	tcp_build_header(skb->h.tcp_header, ntohs(sk->src_sin.sin_port), ntohs(sk->dst_sin.sin_port),
			 sk->seq_no, sk->ack_no, ETHERNET_MAX_MTU, flags);

	/* copy message */
	buf = skb_put(skb, len);
	memcpy(buf, msg, len);

	/* compute tcp checksum */
	skb->h.tcp_header->chksum = tcp_checksum(skb->h.tcp_header, sk->dev->ip_addr, dest_ip, len);

	/* update sequence */
	if (len)
		sk->seq_no += len;
	else if ((flags & TCPCB_FLAG_SYN) || (flags & TCPCB_FLAG_FIN))
		sk->seq_no += 1;

	return skb;
}

/*
 * Reply a ACK message.
 */
static int tcp_reply_ack(struct sock_t *sk, struct sk_buff_t *skb, uint16_t flags)
{
	struct sk_buff_t *skb_ack;
	uint8_t dest_ip[4];
	int len;

	/* get destination IP */
	inet_ntoi(inet_iton(skb->nh.ip_header->src_addr), dest_ip);

	/* allocate a socket buffer */
	skb_ack = skb_alloc(sizeof(struct ethernet_header_t) + sizeof(struct ip_header_t) + sizeof(struct tcp_header_t));
	if (!skb_ack)
		return -ENOMEM;

	/* build ethernet header */
	skb_ack->eth_header = (struct ethernet_header_t *) skb_put(skb_ack, sizeof(struct ethernet_header_t));
	ethernet_build_header(skb_ack->eth_header, sk->dev->mac_addr, NULL, ETHERNET_TYPE_IP);

	/* build ip header */
	skb_ack->nh.ip_header = (struct ip_header_t *) skb_put(skb_ack, sizeof(struct ip_header_t));
	ip_build_header(skb_ack->nh.ip_header, 0, sizeof(struct ip_header_t) + sizeof(struct tcp_header_t), 0,
			IPV4_DEFAULT_TTL, IP_PROTO_TCP, sk->dev->ip_addr, dest_ip);

	/* compute ack number */
	len = tcp_data_length(skb);
	sk->ack_no = ntohl(skb->h.tcp_header->seq) + len;
	if (!len)
		sk->ack_no += 1;

	/* build tcp header */
	skb_ack->h.tcp_header = (struct tcp_header_t *) skb_put(skb_ack, sizeof(struct tcp_header_t));
	tcp_build_header(skb_ack->h.tcp_header, ntohs(sk->src_sin.sin_port), ntohs(skb->h.tcp_header->src_port),
			 sk->seq_no, sk->ack_no, ETHERNET_MAX_MTU, flags);

	/* compute tcp checksum */
	skb_ack->h.tcp_header->chksum = tcp_checksum(skb_ack->h.tcp_header, sk->dev->ip_addr, dest_ip, 0);

	/* transmit ACK message */
	net_transmit(sk->dev, skb_ack);

	/* update sequence */
	if (flags & TCPCB_FLAG_SYN || flags & TCPCB_FLAG_FIN)
		sk->seq_no += 1;

	return 0;
}

/*
 * Reset a TCP connection.
 */
static int tcp_reset(struct sock_t *sk, uint32_t seq_no)
{
	struct sk_buff_t *skb;

	/* set sequence number */
	sk->seq_no = ntohl(seq_no);
	sk->ack_no = 0;

	/* create a RST packet */
	skb = tcp_create_skb(sk, TCPCB_FLAG_RST, NULL, 0);
	if (!skb)
		return -EINVAL;

	/* transmit SYN message */
	net_transmit(sk->dev, skb);

	return 0;
}

/*
 * Handle a TCP packet.
 */
static int tcp_handle(struct sock_t *sk, struct sk_buff_t *skb)
{
	struct sk_buff_t *skb_new;
	uint16_t ack_flags;
	uint32_t data_len;

	/* check protocol */
	if (sk->protocol != skb->nh.ip_header->protocol)
		return -EINVAL;

	/* check destination */
	if (sk->src_sin.sin_port != skb->h.tcp_header->dst_port)
		return -EINVAL;

	/* check source */
	if ((sk->sock->state == SS_CONNECTED || sk->sock->state == SS_CONNECTING)
	    && sk->dst_sin.sin_port != skb->h.tcp_header->src_port)
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
				list_add_tail(&skb_new->list, &sk->skb_list);

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
					list_add_tail(&skb_new->list, &sk->skb_list);
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
	task_wakeup(&sk->sock->wait);

	return 0;
}

/*
 * Receive a TCP message.
 */
static int tcp_recvmsg(struct sock_t *sk, struct msghdr_t *msg, int nonblock, int flags)
{
	size_t len, n, i, count = 0;
	struct sockaddr_in *sin;
	struct sk_buff_t *skb;
	void *buf;

	/* sleep until we receive a packet */
	for (;;) {
		/* dead socket */
		if (sk->sock->state == SS_DEAD)
			return -ENOTCONN;

		/* signal received : restart system call */
		if (signal_pending(current_task))
			return -ERESTARTSYS;

		/* message received : break */
		if (!list_empty(&sk->skb_list))
			break;

		/* disconnected : break */
		if (sk->sock->state == SS_DISCONNECTING)
			return 0;

		/* non blocking */
		if (nonblock)
			return -EAGAIN;

		/* sleep */
		task_sleep(&sk->sock->wait);
	}

	/* get first message */
	skb = list_first_entry(&sk->skb_list, struct sk_buff_t, list);

	/* get IP header */
	skb->nh.ip_header = (struct ip_header_t *) (skb->head + sizeof(struct ethernet_header_t));

	/* get TCP header */
	skb->h.tcp_header = (struct tcp_header_t *) (skb->head + sizeof(struct ethernet_header_t) + sizeof(struct ip_header_t));

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

	/* remove and free socket buffer or remember position packet */
	if (!(flags & MSG_PEEK)) {
		if (len <= 0) {
			list_del(&skb->list);
			skb_free(skb);
			sk->msg_position = 0;
		} else {
			sk->msg_position += count;
		}
	}

	return count;
}

/*
 * Send a TCP message.
 */
static int tcp_sendmsg(struct sock_t *sk, const struct msghdr_t *msg, int nonblock, int flags)
{
	struct sk_buff_t *skb;
	size_t i;
	int len;

	/* unused flags */
	UNUSED(flags);

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
		if (nonblock)
			return -EAGAIN;

		/* sleep */
		task_sleep(&sk->sock->wait);
	}

	for (i = 0, len = 0; i < msg->msg_iovlen; i++) {
		/* create socket buffer */
		skb = tcp_create_skb(sk, TCPCB_FLAG_ACK, msg->msg_iov->iov_base, msg->msg_iov->iov_len);
		if (!skb)
			return -EINVAL;

		/* transmit message */
		net_transmit(sk->dev, skb);

		len += msg->msg_iov->iov_len;
	}

	return len;
}

/*
 * Create a TCP connection.
 */
static int tcp_connect(struct sock_t *sk)
{
	struct sk_buff_t *skb;

	/* generate sequence */
	sk->seq_no = ntohl(rand());
	sk->ack_no = 0;

	/* create SYN message */
	skb = tcp_create_skb(sk, TCPCB_FLAG_SYN, NULL, 0);
	if (!skb)
		return -EINVAL;

	/* transmit SYN message */
	net_transmit(sk->dev, skb);

	/* set socket state */
	sk->sock->state = SS_CONNECTING;

	return 0;
}

/*
 * Accept a TCP connection.
 */
static int tcp_accept(struct sock_t *sk, struct sock_t *sk_new)
{
	struct list_head_t *pos, *n;
	struct sk_buff_t *skb;

	for (;;) {
		/* signal received : restart system call */
		if (signal_pending(current_task))
			return -ERESTARTSYS;

		/* for each received packet */
		list_for_each(pos, &sk->skb_list) {
			/* get socket buffer */
			skb = list_entry(pos, struct sk_buff_t, list);

			/* get IP header */
			skb->nh.ip_header = (struct ip_header_t *) (skb->head + sizeof(struct ethernet_header_t));

			/* get TCP header */
			skb->h.tcp_header = (struct tcp_header_t *) (skb->head + sizeof(struct ethernet_header_t) + sizeof(struct ip_header_t));

			/* not a SYN message : go to next packet */
			if (!skb->h.tcp_header->syn)
				continue;

			/* set new socket */
			sk_new->sock->state = SS_CONNECTED;
			memcpy(&sk_new->src_sin, &sk->src_sin, sizeof(struct sockaddr));
			sk_new->dst_sin.sin_family = AF_INET;
			sk_new->dst_sin.sin_port = skb->h.tcp_header->src_port;
			sk_new->dst_sin.sin_addr = inet_iton(skb->nh.ip_header->src_addr);
			sk_new->seq_no = ntohl(1);
			sk_new->ack_no = ntohl(skb->h.tcp_header->seq) + 1;

			/* free socket buffer */
			list_del(&skb->list);
			skb_free(skb);

			/* move buffers to new socket */
			list_for_each_safe(pos, n, &sk->skb_list) {
				/* decode socket buffer */
				skb = list_entry(pos, struct sk_buff_t, list);
				skb->nh.ip_header = (struct ip_header_t *) (skb->head + sizeof(struct ethernet_header_t));
				skb->h.tcp_header = (struct tcp_header_t *) (skb->head + sizeof(struct ethernet_header_t) + sizeof(struct ip_header_t));

				/* move socket buffer */
				if (sk_new->dst_sin.sin_port == skb->h.tcp_header->src_port
				    && sk_new->dst_sin.sin_addr == inet_iton(skb->nh.ip_header->src_addr)) {
					list_del(&skb->list);
					list_add_tail(&skb->list, &sk_new->skb_list);
				}
			}

			return 0;
		}

		/* disconnected : break */
		if (sk->sock->state == SS_DISCONNECTING)
			return 0;

		/* sleep */
		task_sleep(&sk->sock->wait);
	}

	return 0;
}

/*
 * Close a TCP connection.
 */
static int tcp_close(struct sock_t *sk)
{
	struct sk_buff_t *skb;

	/* socket no connected : no need to send FIN message */
	if (sk->sock->state != SS_CONNECTED) {
		sk->sock->state = SS_DISCONNECTING;
		goto wait_for_ack;
	}

	/* send FIN message */
	skb = tcp_create_skb(sk, TCPCB_FLAG_FIN | TCPCB_FLAG_ACK, NULL, 0);
	if (skb)
		net_transmit(sk->dev, skb);

	/* wait for ACK message */
wait_for_ack:
	while (sk->sock->state != SS_DISCONNECTING) {
		/* signal received : restart system call */
		if (signal_pending(current_task))
			return -ERESTARTSYS;

		/* sleep */
		task_sleep(&sk->sock->wait);
	}

	return 0;
}

/*
 * TCP protocol.
 */
struct proto_t tcp_proto = {
	.handle		= tcp_handle,
	.close		= tcp_close,
	.recvmsg	= tcp_recvmsg,
	.sendmsg	= tcp_sendmsg,
	.connect	= tcp_connect,
	.accept		= tcp_accept,
};
