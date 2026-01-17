#include <net/sock.h>
#include <net/socket.h>
#include <net/inet/udp.h>
#include <net/inet/net.h>
#include <net/inet/tcp.h>
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
	if (sk->sport != skb->h.udp_header->dst_port)
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
	struct sockaddr_in *dest_addr_in = (struct sockaddr_in *) msg->msg_name;
	size_t usize = sizeof(struct udp_header) + size;
	struct udp_fake_header ufh;
	int ret;

	/* build udp header */
	ufh.uh.src_port = sk->sport;
	ufh.uh.dst_port = dest_addr_in->sin_port;
	ufh.uh.len = htons(usize);
	ufh.uh.chksum = 0;
	ufh.iov = msg->msg_iov;

	/* build and transmit ip packet */
	ret = ip_build_xmit(sk, udp_getfrag, &ufh, usize, dest_addr_in->sin_addr, msg->msg_flags);
	if (ret)
		return ret;

	return size;
}

/*
 * Receive an UDP message.
 */
static int udp_recvmsg(struct sock *sk, struct msghdr *msg, size_t size)
{
	struct sockaddr_in *addr_in = (struct sockaddr_in *) msg->msg_name;
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
	if (addr_in) {
		addr_in->sin_family = AF_INET;
		addr_in->sin_port = skb->h.udp_header->src_port;
		addr_in->sin_addr = skb->nh.ip_header->src_addr;
	}

	/* free message */
	skb_free(skb);

	return copied;
}

/*
 * Close a UDP socket.
 */
static int udp_close(struct sock *sk)
{
	sk->state = TCP_CLOSE;
	list_del(&sk->list);
	sk->dead = 1;
	destroy_sock(sk);
	return 0;
}

/*
 * UDP protocol.
 */
struct proto udp_prot = {
	.handle		= udp_handle,
	.recvmsg	= udp_recvmsg,
	.sendmsg	= udp_sendmsg,
	.poll		= datagram_poll,
	.close		= udp_close,
};
