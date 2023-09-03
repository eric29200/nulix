#include <net/sock.h>
#include <net/socket.h>
#include <net/inet/udp.h>
#include <net/inet/net.h>
#include <net/inet/ip.h>
#include <net/inet/ethernet.h>
#include <proc/sched.h>
#include <uio.h>
#include <string.h>
#include <stderr.h>

/*
 * Build an UDP header.
 */
static void udp_build_header(struct udp_header *udp_header, uint16_t src_port, uint16_t dst_port, uint16_t len)
{
	udp_header->src_port = htons(src_port);
	udp_header->dst_port = htons(dst_port);
	udp_header->len = htons(len);
	udp_header->chksum = 0;
}

/*
 * Receive/decode an UDP packet.
 */
void udp_receive(struct sk_buff *skb)
{
	skb->h.udp_header = (struct udp_header *) skb->data;
	skb_pull(skb, sizeof(struct udp_header));
}

/*
 * Handle an UDP packet.
 */
static int udp_handle(struct sock *sk, struct sk_buff *skb)
{
	struct sk_buff *skb_new;

	/* check protocol */
	if (sk->protocol != skb->nh.ip_header->protocol)
		return -EINVAL;

	/* check destination */
	if (sk->protinfo.af_inet.src_sin.sin_port != skb->h.udp_header->dst_port)
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
static int udp_sendmsg(struct sock *sk, const struct msghdr *msg, int nonblock, int flags)
{
	struct sockaddr_in *dest_addr_in;
	struct sk_buff *skb;
	uint8_t dest_ip[4];
	size_t len, i;
	void *buf;

	/* unused flags */
	UNUSED(flags);
	UNUSED(nonblock);

	/* get destination IP */
	dest_addr_in = (struct sockaddr_in *) msg->msg_name;
	inet_ntoi(dest_addr_in->sin_addr, dest_ip);

	/* compute data length */
	len = 0;
	for (i = 0; i < msg->msg_iovlen; i++)
		len += msg->msg_iov[i].iov_len;

	/* allocate a socket buffer */
	skb = skb_alloc(sizeof(struct ethernet_header) + sizeof(struct ip_header) + sizeof(struct udp_header) + len);
	if (!skb)
		return -ENOMEM;

	/* build ethernet header */
	skb->eth_header = (struct ethernet_header *) skb_put(skb, sizeof(struct ethernet_header));
	ethernet_build_header(skb->eth_header, sk->protinfo.af_inet.dev->mac_addr, NULL, ETHERNET_TYPE_IP);

	/* build ip header */
	skb->nh.ip_header = (struct ip_header *) skb_put(skb, sizeof(struct ip_header));
	ip_build_header(skb->nh.ip_header, 0, sizeof(struct ip_header) + sizeof(struct udp_header) + len, 0,
			IPV4_DEFAULT_TTL, IP_PROTO_UDP, sk->protinfo.af_inet.dev->ip_addr, dest_ip);

	/* build udp header */
	skb->h.udp_header = (struct udp_header *) skb_put(skb, sizeof(struct udp_header));
	udp_build_header(skb->h.udp_header, ntohs(sk->protinfo.af_inet.src_sin.sin_port), ntohs(dest_addr_in->sin_port), sizeof(struct udp_header) + len);

	/* copy message */
	buf = skb_put(skb, len);
	for (i = 0; i < msg->msg_iovlen; i++) {
		memcpy(buf, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
		buf += msg->msg_iov[i].iov_len;
	}

	/* transmit message */
	net_transmit(sk->protinfo.af_inet.dev, skb);

	return len;
}

/*
 * Receive an UDP message.
 */
static int udp_recvmsg(struct sock *sk, struct msghdr *msg, int nonblock, int flags)
{
	size_t len, n, i, count = 0;
	struct sockaddr_in *sin;
	struct sk_buff *skb;
	void *buf;

	/* sleep until we receive a packet */
	for (;;) {
		/* signal received : restart system call */
		if (signal_pending(current_task))
			return -ERESTARTSYS;

		/* message received : break */
		if (!list_empty(&sk->skb_list))
			break;

		/* non blocking */
		if (nonblock)
			return -EAGAIN;

		/* sleep */
		task_sleep(&sk->sock->wait);
	}

	/* get first message */
	skb = list_first_entry(&sk->skb_list, struct sk_buff, list);

	/* get IP header */
	skb->nh.ip_header = (struct ip_header *) (skb->head + sizeof(struct ethernet_header));

	/* get UDP header */
	skb->h.udp_header = (struct udp_header *) (skb->head + sizeof(struct ethernet_header) + sizeof(struct ip_header));

	/* get message */
	buf = (void *) skb->h.udp_header + sizeof(struct udp_header) + sk->msg_position;
	len = (void *) skb->end - buf;

	/* copy message */
	for (i = 0; i < msg->msg_iovlen; i++) {
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
		sin->sin_port = skb->h.udp_header->src_port;
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
 * UDP protocol.
 */
struct proto udp_proto = {
	.handle		= udp_handle,
	.recvmsg	= udp_recvmsg,
	.sendmsg	= udp_sendmsg,
};
