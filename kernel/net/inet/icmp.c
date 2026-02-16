#include <net/sock.h>
#include <net/inet/icmp.h>
#include <net/inet/net.h>
#include <net/inet/ip.h>
#include <net/inet/tcp.h>
#include <proc/sched.h>
#include <uio.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>
#include <fcntl.h>

/* static icmp socket, to send messages */
static struct inode icmp_inode;
static struct socket *icmp_socket = &icmp_inode.u.socket_i;

/*
 * ICMP fake header.
 */
struct icmp_fake_header {
	struct icmp_header	icmph;
	struct iovec *		iov;
};

/*
 * Copy a ICMP fragment.
 */
static void icmp_getfrag(const void *ptr, char *to, size_t fraglen)
{
	struct icmp_fake_header *icmpfh = (struct icmp_fake_header *) ptr;
	struct icmp_header *icmph;

	/* copy header and data */
	memcpy(to, &icmpfh->icmph, sizeof(struct icmp_header));
	memcpy_fromiovec((uint8_t *) to + sizeof(struct icmp_header), icmpfh->iov, fraglen - sizeof(struct icmp_header));

	/* recompute checksum */
	icmph = (struct icmp_header *) to;
	icmph->chksum = net_checksum(icmph, sizeof(struct icmp_header) + fraglen);
}

/*
 * Reply to a echo request.
 */
static void icmp_reply_echo(struct sk_buff *skb)
{
	uint32_t daddr = skb->nh.ip_header->src_addr;
	struct sock *sk = icmp_socket->sk;
	struct icmp_fake_header icmpfh;
	struct iovec iov;

	/* build icmp header */
	memcpy(&icmpfh.icmph, skb->h.icmp_header, sizeof(struct icmp_header));
	icmpfh.icmph.type = ICMP_TYPE_ECHO_REPLY;
	icmpfh.icmph.chksum = 0;
	iov.iov_base = (uint8_t *) skb->h.icmp_header + sizeof(struct icmp_header);
	iov.iov_len = ntohs(skb->nh.ip_header->length) - sizeof(struct ip_header);
	icmpfh.iov = &iov;

	/* send reply */
	ip_build_xmit(sk, icmp_getfrag, &icmpfh, iov.iov_len + sizeof(struct icmp_header), daddr, 0);
}

/*
 * Receive/decode an ICMP packet.
 */
void icmp_receive(struct sk_buff *skb)
{
	/* decode ICMP header */
	skb->h.icmp_header = (struct icmp_header *) skb->data;
	skb_pull(skb, sizeof(struct icmp_header));
	skb->h.icmp_header->chksum = 0;
	skb->h.icmp_header->chksum = net_checksum(skb->h.icmp_header, skb->size - skb->dev->hard_header_len - sizeof(struct ip_header));

	/* reply to echo request */
	if (skb->h.icmp_header->type == ICMP_TYPE_ECHO)
		icmp_reply_echo(skb);
}

/*
 * Init ICMP protocol.
 */
void icmp_init(struct net_proto_family *ops)
{
	int ret;

	/* init static socket */
	icmp_inode.i_mode = S_IFSOCK;
	icmp_inode.i_sock = 1;
	icmp_inode.i_uid = 0;
	icmp_inode.i_gid = 0;
	icmp_socket->inode = &icmp_inode;
	icmp_socket->state = SS_UNCONNECTED;
	icmp_socket->type = SOCK_RAW;

	/* create icmp socket */
	ret = ops->create(icmp_socket, IP_PROTO_ICMP);
	if (ret)
		panic("icmp_init: dailed to create the ICMP control socket");
}

/*
 * Handle a ICMP packet.
 */
static int icmp_handle(struct sock *sk, struct sk_buff *skb)
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
 * Receive a ICMP message.
 */
static int icmp_recvmsg(struct sock *sk, struct msghdr *msg, size_t size)
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
	buf = (void *) skb->h.raw + sizeof(struct icmp_header) + sk->msg_position;
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
		addr_in->sin_port = 0;
		addr_in->sin_addr = skb->nh.ip_header->src_addr;
	}

	/* free message */
	skb_free(skb);

	return copied;
}

/*
 * Send a ICMP message.
 */
static int icmp_sendmsg(struct sock *sk, const struct msghdr *msg, size_t size)
{
	struct sockaddr_in *dest_addr_in = (struct sockaddr_in *) msg->msg_name;
	size_t isize = sizeof(struct icmp_header) + size;
	struct icmp_fake_header icmpfh;
	struct icmp_header user_icmph;
	uint32_t daddr;
	int ret;

	/* check message size */
	if (size > 0xFFFF)
		return -EMSGSIZE;

	/* get destination */
	if (dest_addr_in) {
		if (msg->msg_namelen < sizeof(struct sockaddr_in))
			return -EINVAL;
		if (dest_addr_in->sin_family && dest_addr_in->sin_family != AF_INET)
			return -EINVAL;

		daddr = dest_addr_in->sin_addr;
	} else {
		if (sk->state != TCP_ESTABLISHED)
			return -EINVAL;

		daddr = sk->daddr;
	}

	/* get user icmp header */
	memcpy_fromiovec((uint8_t *) &user_icmph, msg->msg_iov, sizeof(struct icmp_header));

	/* build icmp header */
	icmpfh.icmph.code = user_icmph.code;
	icmpfh.icmph.type = user_icmph.type;
	icmpfh.icmph.un.echo.id = sk->sport;
	icmpfh.icmph.un.echo.sequence = user_icmph.un.echo.sequence;
	icmpfh.icmph.chksum = 0;
	icmpfh.iov = msg->msg_iov;

	/* build and transmit ip packet */
	ret = ip_build_xmit(sk, icmp_getfrag, &icmpfh, isize, daddr, msg->msg_flags);
	if (ret)
		return ret;

	return size;
}

/*
 * Close a ICMP socket.
 */
static int icmp_close(struct sock *sk)
{
	sk->state = TCP_CLOSE;
	list_del(&sk->list);
	sk->dead = 1;
	destroy_sock(sk);
	return 0;
}

/*
 * ICMP protocol.
 */
struct proto icmp_prot = {
	.handle		= icmp_handle,
	.connect	= datagram_connect,
	.recvmsg	= icmp_recvmsg,
	.sendmsg	= icmp_sendmsg,
	.poll		= datagram_poll,
	.close		= icmp_close,
	.getsockopt	= ip_getsockopt,
	.setsockopt	= ip_setsockopt,
};