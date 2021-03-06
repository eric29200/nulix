#include <net/tcp.h>
#include <net/socket.h>
#include <net/ip.h>
#include <net/arp.h>
#include <net/ethernet.h>
#include <proc/sched.h>
#include <math.h>
#include <string.h>
#include <stderr.h>

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
static struct sk_buff_t *tcp_create_skb(struct socket_t *sock, uint16_t flags, void *msg, size_t len)
{
	struct sk_buff_t *skb;
	uint8_t dest_ip[4];
	void *buf;

	/* get destination IP */
	inet_ntoi(sock->dst_sin.sin_addr, dest_ip);

	/* allocate a socket buffer */
	skb = skb_alloc(sizeof(struct ethernet_header_t) + sizeof(struct ip_header_t) + sizeof(struct tcp_header_t) + len);
	if (!skb)
		return NULL;

	/* build ethernet header */
	skb->eth_header = (struct ethernet_header_t *) skb_put(skb, sizeof(struct ethernet_header_t));
	ethernet_build_header(skb->eth_header, sock->dev->mac_addr, NULL, ETHERNET_TYPE_IP);

	/* build ip header */
	skb->nh.ip_header = (struct ip_header_t *) skb_put(skb, sizeof(struct ip_header_t));
	ip_build_header(skb->nh.ip_header, 0, sizeof(struct ip_header_t) + sizeof(struct tcp_header_t) + len, 0,
			IPV4_DEFAULT_TTL, IP_PROTO_TCP, sock->dev->ip_addr, dest_ip);

	/* build tcp header */
	skb->h.tcp_header = (struct tcp_header_t *) skb_put(skb, sizeof(struct tcp_header_t));
	tcp_build_header(skb->h.tcp_header, ntohs(sock->src_sin.sin_port), ntohs(sock->dst_sin.sin_port),
			 sock->seq_no, sock->ack_no, ETHERNET_MAX_MTU, flags);

	/* copy message */
	buf = skb_put(skb, len);
	memcpy(buf, msg, len);

	/* compute tcp checksum */
	skb->h.tcp_header->chksum = tcp_checksum(skb->h.tcp_header, sock->dev->ip_addr, dest_ip, len);

	/* update sequence */
	if (len)
		sock->seq_no += len;
	else if (flags & TCPCB_FLAG_SYN)
		sock->seq_no += 1;

	return skb;
}

/*
 * Reply a ACK message.
 */
static int tcp_reply_ack(struct socket_t *sock, struct sk_buff_t *skb, uint16_t flags)
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
	ethernet_build_header(skb_ack->eth_header, sock->dev->mac_addr, NULL, ETHERNET_TYPE_IP);

	/* build ip header */
	skb_ack->nh.ip_header = (struct ip_header_t *) skb_put(skb_ack, sizeof(struct ip_header_t));
	ip_build_header(skb_ack->nh.ip_header, 0, sizeof(struct ip_header_t) + sizeof(struct tcp_header_t), 0,
			IPV4_DEFAULT_TTL, IP_PROTO_TCP, sock->dev->ip_addr, dest_ip);

	/* compute ack number */
	len = tcp_data_length(skb);
	sock->ack_no = ntohl(skb->h.tcp_header->seq) + len;
	if (!len)
		sock->ack_no += 1;

	/* build tcp header */
	skb_ack->h.tcp_header = (struct tcp_header_t *) skb_put(skb_ack, sizeof(struct tcp_header_t));
	tcp_build_header(skb_ack->h.tcp_header, ntohs(sock->src_sin.sin_port), ntohs(skb->h.tcp_header->src_port),
			 sock->seq_no, sock->ack_no, ETHERNET_MAX_MTU, flags);

	/* compute tcp checksum */
	skb_ack->h.tcp_header->chksum = tcp_checksum(skb_ack->h.tcp_header, sock->dev->ip_addr, dest_ip, 0);

	/* transmit ACK message */
	net_transmit(sock->dev, skb_ack);

	/* update sequence */
	if (flags & TCPCB_FLAG_SYN || flags & TCPCB_FLAG_FIN)
		sock->seq_no += 1;

	return 0;
}

/*
 * Handle a TCP packet.
 */
int tcp_handle(struct socket_t *sock, struct sk_buff_t *skb)
{
	struct sk_buff_t *skb_new;
	uint16_t ack_flags;
	uint32_t data_len;

	/* check protocol */
	if (sock->protocol != skb->nh.ip_header->protocol)
		return -EINVAL;

	/* check destination */
	if (sock->src_sin.sin_port != skb->h.tcp_header->dst_port)
		return -EINVAL;

	/* check source */
	if ((sock->state == SS_CONNECTED || sock->state == SS_CONNECTING)
	    && sock->dst_sin.sin_port != skb->h.tcp_header->src_port)
		return -EINVAL;

	/* compute data length */
	data_len = tcp_data_length(skb);

	/* set ack flags */
	ack_flags = TCPCB_FLAG_ACK;
	if (skb->h.tcp_header->syn && !skb->h.tcp_header->ack)
		ack_flags |= TCPCB_FLAG_SYN;
	else if (skb->h.tcp_header->fin)
		ack_flags |= TCPCB_FLAG_FIN;

	/* send ACK message */
	if (data_len > 0 || skb->h.tcp_header->syn || skb->h.tcp_header->fin)
		tcp_reply_ack(sock, skb, ack_flags);

	/* handle TCP packet */
	switch (sock->state) {
		case SS_LISTENING:
			/* clone socket buffer */
			skb_new = skb_clone(skb);
			if (!skb_new)
				return -ENOMEM;

			/* add buffer to socket */
			list_add_tail(&skb_new->list, &sock->skb_list);

			break;
		case SS_CONNECTING:
			/* find SYN | ACK message */
			if (skb->h.tcp_header->syn && skb->h.tcp_header->ack)
				sock->state = SS_CONNECTED;

			break;
		case SS_CONNECTED:
				/* add message */
				if (data_len > 0) {
					/* clone socket buffer */
					skb_new = skb_clone(skb);
					if (!skb_new)
						return -ENOMEM;

					/* add buffer to socket */
					list_add_tail(&skb_new->list, &sock->skb_list);
				}

				/* FIN message : close socket */
				if (skb->h.tcp_header->fin)
					sock->state = SS_DISCONNECTING;

			break;
		default:
			break;
	}

	/* wake up eventual processes */
	task_wakeup(&sock->waiting_chan);

	return 0;
}

/*
 * Receive a TCP message.
 */
int tcp_recvmsg(struct socket_t *sock, struct msghdr_t *msg, int flags)
{
	size_t len, n, i, count = 0;
	struct sockaddr_in *sin;
	struct sk_buff_t *skb;
	void *buf;

	/* unused flags */
	UNUSED(flags);

	/* sleep until we receive a packet */
	for (;;) {
		/* signal received : restart system call */
		if (!sigisemptyset(&current_task->sigpend))
			return -ERESTARTSYS;

		/* message received : break */
		if (!list_empty(&sock->skb_list))
			break;

		/* disconnected : break */
		if (sock->state == SS_DISCONNECTING)
			return 0;

		/* sleep */
		task_sleep(&sock->waiting_chan);
	}

	/* get first message */
	skb = list_first_entry(&sock->skb_list, struct sk_buff_t, list);

	/* get IP header */
	skb->nh.ip_header = (struct ip_header_t *) (skb->head + sizeof(struct ethernet_header_t));

	/* get TCP header */
	skb->h.tcp_header = (struct tcp_header_t *) (skb->head + sizeof(struct ethernet_header_t) + sizeof(struct ip_header_t));

	/* get message */
	buf = tcp_data(skb) + sock->msg_position;
	len = tcp_data_length(skb) - sock->msg_position;

	/* copy message */
	for (i = 0; i < msg->msg_iovlen && len > 0; i++) {
		n = len > msg->msg_iov[i].iov_len ? msg->msg_iov[i].iov_len : len;
		memcpy(msg->msg_iov[i].iov_base, buf, n);
		count += n;
		len -= n;
	}

	/* remove and free socket buffer or remember position packet */
	if (len <= 0) {
		list_del(&skb->list);
		skb_free(skb);
		sock->msg_position = 0;
	} else {
		sock->msg_position = count;
	}

	/* set source address */
	sin = (struct sockaddr_in *) msg->msg_name;
	sin->sin_family = AF_INET;
	sin->sin_port = skb->h.tcp_header->src_port;
	sin->sin_addr = inet_iton(skb->nh.ip_header->src_addr);

	return count;
}

/*
 * Send a TCP message.
 */
int tcp_sendmsg(struct socket_t *sock, const struct msghdr_t *msg, int flags)
{
	struct sk_buff_t *skb;
	size_t i;
	int len;

	/* unused flags */
	UNUSED(flags);

	/* sleep until connected */
	for (;;) {
		/* signal received : restart system call */
		if (!sigisemptyset(&current_task->sigpend))
			return -ERESTARTSYS;

		/* connected : break */
		if (sock->state == SS_CONNECTED)
			break;

		/* sleep */
		task_sleep(&sock->waiting_chan);
	}

	for (i = 0, len = 0; i < msg->msg_iovlen; i++) {
		/* create socket buffer */
		skb = tcp_create_skb(sock, TCPCB_FLAG_ACK, msg->msg_iov->iov_base, msg->msg_iov->iov_len);
		if (!skb)
			return -EINVAL;

		/* transmit message */
		net_transmit(sock->dev, skb);

		len += msg->msg_iov->iov_len;
	}

	return len;
}

/*
 * Create a TCP connection.
 */
int tcp_connect(struct socket_t *sock)
{
	struct sk_buff_t *skb;

	/* generate sequence */
	sock->seq_no = rand();
	sock->ack_no = 0;

	/* create SYN message */
	skb = tcp_create_skb(sock, TCPCB_FLAG_SYN, NULL, 0);
	if (!skb)
		return -EINVAL;

	/* transmit SYN message */
	net_transmit(sock->dev, skb);

	/* set socket state */
	sock->state = SS_CONNECTING;

	return 0;
}

/*
 * Accept a TCP connection.
 */
int tcp_accept(struct socket_t *sock, struct socket_t *sock_new)
{
	struct list_head_t *pos;
	struct sk_buff_t *skb;

	for (;;) {
		/* signal received : restart system call */
		if (!sigisemptyset(&current_task->sigpend))
			return -ERESTARTSYS;

		/* for each received packet */
		list_for_each(pos, &sock->skb_list) {
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
			sock_new->state = SS_CONNECTED;
			memcpy(&sock_new->src_sin, &sock->src_sin, sizeof(struct sockaddr));
			sock_new->dst_sin.sin_family = AF_INET;
			sock_new->dst_sin.sin_port = skb->h.tcp_header->src_port;
			sock_new->dst_sin.sin_addr = inet_iton(skb->nh.ip_header->src_addr);
			sock_new->seq_no = 1;
			sock_new->ack_no = ntohl(skb->h.tcp_header->seq) + 1;

			/* free socket buffer */
			list_del(&skb->list);
			skb_free(skb);

			return 0;
		}

		/* disconnected : break */
		if (sock->state == SS_DISCONNECTING)
			return 0;

		/* sleep */
		task_sleep(&sock->waiting_chan);
	}

	return 0;
}

/*
 * Close a TCP connection.
 */
static int tcp_close(struct socket_t *sock)
{
	struct sk_buff_t *skb;

	/* send FIN message */
	skb = tcp_create_skb(sock, TCPCB_FLAG_FIN | TCPCB_FLAG_ACK, NULL, 0);
	if (skb)
		net_transmit(sock->dev, skb);

	return 0;
}

/*
 * TCP protocol operations.
 */
struct prot_ops tcp_prot_ops = {
	.handle		= tcp_handle,
	.recvmsg	= tcp_recvmsg,
	.sendmsg	= tcp_sendmsg,
	.connect	= tcp_connect,
	.accept		= tcp_accept,
	.close		= tcp_close,
};

