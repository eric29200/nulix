#include <net/sock.h>
#include <net/socket.h>
#include <net/inet/udp.h>
#include <net/inet/net.h>
#include <net/inet/ip.h>
#include <proc/sched.h>
#include <uio.h>
#include <stdio.h>
#include <string.h>
#include <stderr.h>

/*
 * UDP fake header.
 */
struct udp_fake_header {
	struct udp_header	uh;
	struct iovec *		iov;
};

/*
 * Copy a UDP fragment.
 */
static void udp_getfrag(const void *ptr, char *to, size_t fraglen)
{
	struct udp_fake_header *ufh = (struct udp_fake_header *) ptr;
	memcpy(to, &ufh->uh, sizeof(struct udp_header));
	memcpy_fromiovec((uint8_t *) to + sizeof(struct udp_header), ufh->iov, fraglen - sizeof(struct udp_header));
}

/*
 * Receive/decode an UDP packet.
 */
void udp_receive(struct sk_buff *skb)
{
	/* decode UDP header */
	skb->h.udp_header = (struct udp_header *) skb->data;
	skb_pull(skb, sizeof(struct udp_header));

	/* deliver packet to sockets */
	net_deliver_skb(skb);
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
	size_t usize = sizeof(struct udp_header) + size;
	struct sockaddr_in *dest_addr_in;
	struct udp_fake_header ufh;
	uint32_t dest_ip;
	int ret;

	/* get destination IP */
	dest_addr_in = (struct sockaddr_in *) msg->msg_name;
	dest_ip = dest_addr_in->sin_addr;

	/* build udp header */
	ufh.uh.src_port = sk->protinfo.af_inet.src_sin.sin_port;
	ufh.uh.dst_port = dest_addr_in->sin_port;
	ufh.uh.len = htons(usize);
	ufh.uh.chksum = 0;
	ufh.iov = msg->msg_iov;

	/* build and transmit ip packet */
	ret = ip_build_xmit(sk, udp_getfrag, &ufh, usize, dest_ip);
	if (ret)
		return ret;

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

	/* get message */
	buf = (void *) skb->h.raw + sizeof(struct udp_header) + sk->msg_position;
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
