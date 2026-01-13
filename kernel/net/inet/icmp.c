#include <net/sock.h>
#include <net/inet/icmp.h>
#include <net/inet/net.h>
#include <net/inet/ip.h>
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
	void *			data_ptr;
	size_t			data_len;
};

/*
 * Copy a ICMP fragment.
 */
static void icmp_getfrag(const void *ptr, char *to,size_t fraglen)
{
	struct icmp_fake_header *icmpfh = (struct icmp_fake_header *) ptr;
	struct icmp_header *icmph;

	/* copy header and data */
	memcpy(to, &icmpfh->icmph, sizeof(struct icmp_header));
	memcpy(to + sizeof(struct icmp_header), icmpfh->data_ptr, fraglen - sizeof(struct icmp_header));

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

	/* build icmp header */
	memcpy(&icmpfh.icmph, skb->h.icmp_header, sizeof(struct icmp_header));
	icmpfh.icmph.type = ICMP_TYPE_ECHO_REPLY;
	icmpfh.icmph.chksum = 0;
	icmpfh.data_ptr = (uint8_t *) skb->h.icmp_header + sizeof(struct icmp_header);
	icmpfh.data_len = ntohs(skb->nh.ip_header->length) - sizeof(struct ip_header);

	/* send reply */
	ip_build_xmit(sk, icmp_getfrag, &icmpfh, icmpfh.data_len + sizeof(struct icmp_header), daddr);
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