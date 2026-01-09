#include <net/sock.h>
#include <net/inet/net.h>
#include <net/inet/ip.h>
#include <proc/sched.h>
#include <uio.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Raw fake header.
 */
struct raw_fake_header {
	struct iovec *		iov;
};

/*
 * Copy a raw fragment.
 */
static void raw_getfrag(const void *ptr, char *to, size_t fraglen)
{
	struct raw_fake_header *rfh = (struct raw_fake_header *) ptr;
	memcpy_fromiovec((uint8_t *) to, rfh->iov, fraglen);
}

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

	/* get message */
	buf = (void *) skb->h.raw + sk->msg_position;
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
	struct raw_fake_header rfh;
	uint32_t dest_ip;
	int ret;

	/* get destination IP */
	dest_addr_in = (struct sockaddr_in *) msg->msg_name;
	dest_ip = dest_addr_in->sin_addr;

	/* build raw header */
	rfh.iov = msg->msg_iov;

	/* build and transmit ip packet */
	ret = ip_build_xmit(sk, raw_getfrag, &rfh, size, dest_ip);
	if (ret)
		return ret;

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
