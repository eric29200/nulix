#include <net/sock.h>
#include <net/inet/net.h>
#include <net/inet/ethernet.h>
#include <net/inet/ip.h>
#include <proc/sched.h>
#include <uio.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Handle a raw packet.
 */
static int raw_handle(struct sock *sk, struct sk_buff *skb)
{
	struct sk_buff *skb_new;

	/* check protocol */
	if (sk->protocol != skb->nh.ip_header->protocol)
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
 * Receive a rax message.
 */
static int raw_recvmsg(struct sock *sk, struct msghdr *msg, size_t size)
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

	/* get message */
	buf = skb->nh.ip_header + sk->msg_position;
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
		sin->sin_port = 0;
		sin->sin_addr = skb->nh.ip_header->src_addr;
	}

	/* free message */
	skb_free(skb);

	return copied;
}

/*
 * Send a raw message.
 */
static int raw_sendmsg(struct sock *sk, const struct msghdr *msg, size_t size)
{
	struct sockaddr_in *dest_addr_in;
	struct sk_buff *skb;
	uint32_t dest_ip;

	/* get destination IP */
	dest_addr_in = (struct sockaddr_in *) msg->msg_name;
	dest_ip = dest_addr_in->sin_addr;

	/* allocate a socket buffer */
	skb = skb_alloc(sizeof(struct ethernet_header) + sizeof(struct ip_header) + size);
	if (!skb)
		return -ENOMEM;

	/* build ethernet header */
	skb->hh.eth_header = (struct ethernet_header *) skb_put(skb, sizeof(struct ethernet_header));
	ethernet_build_header(skb->hh.eth_header, sk->protinfo.af_inet.dev->mac_addr, NULL, ETHERNET_TYPE_IP);

	/* build ip header */
	skb->nh.ip_header = (struct ip_header *) skb_put(skb, sizeof(struct ip_header));
	ip_build_header(skb->nh.ip_header, 0, sizeof(struct ip_header) + size, 0, IPV4_DEFAULT_TTL,
		sk->protocol, sk->protinfo.af_inet.dev->ip_addr, dest_ip);

	/* copy ip message */
	memcpy_fromiovec(skb_put(skb, size), msg->msg_iov, size);

	/* send message */
	net_transmit(sk->protinfo.af_inet.dev, skb);

	return size;
}

/*
 * Raw protocol.
 */
struct proto raw_proto = {
	.handle		= raw_handle,
	.recvmsg	= raw_recvmsg,
	.sendmsg	= raw_sendmsg,
};
