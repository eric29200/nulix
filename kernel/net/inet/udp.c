#include <net/inet/udp.h>
#include <net/inet/net.h>
#include <net/inet/ip.h>
#include <net/inet/ethernet.h>
#include <net/inet/sock.h>
#include <proc/sched.h>
#include <uio.h>
#include <string.h>
#include <stderr.h>

/*
 * Build an UDP header.
 */
static void udp_build_header(struct udp_header_t *udp_header, uint16_t src_port, uint16_t dst_port, uint16_t len)
{
	udp_header->src_port = htons(src_port);
	udp_header->dst_port = htons(dst_port);
	udp_header->len = htons(len);
	udp_header->chksum = 0;
}

/*
 * Receive/decode an UDP packet.
 */
void udp_receive(struct sk_buff_t *skb)
{
	skb->h.udp_header = (struct udp_header_t *) skb->data;
	skb_pull(skb, sizeof(struct udp_header_t));
}

/*
 * Handle an UDP packet.
 */
static int udp_handle(struct sock_t *sk, struct sk_buff_t *skb)
{
	struct sk_buff_t *skb_new;

	/* check protocol */
	if (sk->protocol != skb->nh.ip_header->protocol)
		return -EINVAL;

	/* check destination */
	if (sk->src_sin.sin_port != skb->h.udp_header->dst_port)
		return -EINVAL;

	/* clone socket buffer */
	skb_new = skb_clone(skb);
	if (!skb_new)
		return -ENOMEM;

	/* push skb in socket queue */
	list_add_tail(&skb_new->list, &sk->skb_list);

	return 0;
}

/*
 * Send an UDP message.
 */
static int udp_sendmsg(struct sock_t *sk, const struct msghdr_t *msg, int flags)
{
	struct sockaddr_in *dest_addr_in;
	struct sk_buff_t *skb;
	uint8_t dest_ip[4];
	size_t len, i;
	void *buf;

	/* unused flags */
	UNUSED(flags);

	/* get destination IP */
	dest_addr_in = (struct sockaddr_in *) msg->msg_name;
	inet_ntoi(dest_addr_in->sin_addr, dest_ip);

	/* compute data length */
	len = 0;
	for (i = 0; i < msg->msg_iovlen; i++)
		len += msg->msg_iov[i].iov_len;

	/* allocate a socket buffer */
	skb = skb_alloc(sizeof(struct ethernet_header_t) + sizeof(struct ip_header_t) + sizeof(struct udp_header_t) + len);
	if (!skb)
		return -ENOMEM;

	/* build ethernet header */
	skb->eth_header = (struct ethernet_header_t *) skb_put(skb, sizeof(struct ethernet_header_t));
	ethernet_build_header(skb->eth_header, sk->dev->mac_addr, NULL, ETHERNET_TYPE_IP);

	/* build ip header */
	skb->nh.ip_header = (struct ip_header_t *) skb_put(skb, sizeof(struct ip_header_t));
	ip_build_header(skb->nh.ip_header, 0, sizeof(struct ip_header_t) + sizeof(struct udp_header_t) + len, 0,
			IPV4_DEFAULT_TTL, IP_PROTO_UDP, sk->dev->ip_addr, dest_ip);

	/* build udp header */
	skb->h.udp_header = (struct udp_header_t *) skb_put(skb, sizeof(struct udp_header_t));
	udp_build_header(skb->h.udp_header, ntohs(sk->src_sin.sin_port), ntohs(dest_addr_in->sin_port), sizeof(struct udp_header_t) + len);

	/* copy message */
	buf = skb_put(skb, len);
	for (i = 0; i < msg->msg_iovlen; i++) {
		memcpy(buf, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
		buf += msg->msg_iov[i].iov_len;
	}

	/* transmit message */
	net_transmit(sk->dev, skb);

	return len;
}

/*
 * Receive an UDP message.
 */
static int udp_recvmsg(struct sock_t *sk, struct msghdr_t *msg, int flags)
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
		if (!list_empty(&sk->skb_list))
			break;

		/* sleep */
		task_sleep(&sk->sock->waiting_chan);
	}

	/* get first message */
	skb = list_first_entry(&sk->skb_list, struct sk_buff_t, list);

	/* get IP header */
	skb->nh.ip_header = (struct ip_header_t *) (skb->head + sizeof(struct ethernet_header_t));

	/* get UDP header */
	skb->h.udp_header = (struct udp_header_t *) (skb->head + sizeof(struct ethernet_header_t) + sizeof(struct ip_header_t));

	/* get message */
	buf = (void *) skb->h.udp_header + sizeof(struct udp_header_t);

	/* compute message length */
	len = (void *) skb->end - buf;

	/* copy message */
	for (i = 0; i < msg->msg_iovlen; i++) {
		n = len > msg->msg_iov[i].iov_len ? msg->msg_iov[i].iov_len : len;
		memcpy(msg->msg_iov[i].iov_base, buf, n);
		count += n;
	}

	/* set source address */
	sin = (struct sockaddr_in *) msg->msg_name;
	sin->sin_family = AF_INET;
	sin->sin_port = skb->h.udp_header->src_port;
	sin->sin_addr = inet_iton(skb->nh.ip_header->src_addr);

	/* remove and free socket buffer */
	list_del(&skb->list);
	skb_free(skb);

	return count;
}

/*
 * UDP protocol.
 */
struct proto_t udp_proto = {
	.handle		= udp_handle,
	.recvmsg	= udp_recvmsg,
	.sendmsg	= udp_sendmsg,
};
