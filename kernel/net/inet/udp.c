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
	skb_queue_tail(&sk->receive_queue, skb_new);

	return 0;
}

/*
 * Send an UDP message.
 */
static int udp_sendmsg(struct sock *sk, const struct msghdr *msg, size_t size)
{
	struct sockaddr_in *dest_addr_in;
	struct sk_buff *skb;
	uint32_t dest_ip;

	/* get destination IP */
	dest_addr_in = (struct sockaddr_in *) msg->msg_name;
	dest_ip = dest_addr_in->sin_addr;

	/* allocate a socket buffer */
	skb = skb_alloc(sizeof(struct ethernet_header) + sizeof(struct ip_header) + sizeof(struct udp_header) + size);
	if (!skb)
		return -ENOMEM;

	/* build ethernet header */
	skb->hh.eth_header = (struct ethernet_header *) skb_put(skb, sizeof(struct ethernet_header));
	ethernet_build_header(skb->hh.eth_header, sk->protinfo.af_inet.dev->mac_addr, NULL, ETHERNET_TYPE_IP);

	/* build ip header */
	skb->nh.ip_header = (struct ip_header *) skb_put(skb, sizeof(struct ip_header));
	ip_build_header(skb->nh.ip_header, 0, sizeof(struct ip_header) + sizeof(struct udp_header) + size, 0,
			IPV4_DEFAULT_TTL, IP_PROTO_UDP, sk->protinfo.af_inet.dev->ip_addr, dest_ip);

	/* build udp header */
	skb->h.udp_header = (struct udp_header *) skb_put(skb, sizeof(struct udp_header));
	udp_build_header(skb->h.udp_header, ntohs(sk->protinfo.af_inet.src_sin.sin_port), ntohs(dest_addr_in->sin_port), sizeof(struct udp_header) + size);

	/* copy message */
	memcpy_fromiovec(skb_put(skb, size), msg->msg_iov, size);

	/* transmit message */
	net_transmit(sk->protinfo.af_inet.dev, skb);

	return size;
}

/*
 * Receive an UDP message.
 */
static int udp_recvmsg(struct sock *sk, struct msghdr *msg, size_t size)
{
	struct sockaddr_in *sin;
	struct sk_buff *skb;
	size_t copied;
	void *buf;
	int err;

	/* receive a packet */
	skb = skb_recv_datagram(sk, msg->msg_flags, msg->msg_flags & MSG_DONTWAIT, &err);
	if (!skb)
 		return err;

	/* get IP header */
	skb->nh.ip_header = (struct ip_header *) (skb->head + sizeof(struct ethernet_header));

	/* get UDP header */
	skb->h.udp_header = (struct udp_header *) (skb->head + sizeof(struct ethernet_header) + sizeof(struct ip_header));

	/* get message */
	buf = (void *) skb->h.udp_header + sizeof(struct udp_header) + sk->msg_position;
	copied = (void *) skb->end - buf;
	if (size < copied) {
		copied = size;
		msg->msg_flags |= MSG_TRUNC;
	}

	/* copy message */
	memcpy_toiovec(msg->msg_iov, buf, copied);

	/* set source address */
	sin = (struct sockaddr_in *) msg->msg_name;
	if (sin) {
		sin->sin_family = AF_INET;
		sin->sin_port = skb->h.udp_header->src_port;
		sin->sin_addr = skb->nh.ip_header->src_addr;
	}

	/* free message */
	skb_free(skb);

	return copied;
}

/*
 * UDP protocol.
 */
struct proto udp_proto = {
	.handle		= udp_handle,
	.recvmsg	= udp_recvmsg,
	.sendmsg	= udp_sendmsg,
};
