#include <net/sock.h>
#include <net/inet/icmp.h>
#include <net/inet/net.h>
#include <net/inet/ethernet.h>
#include <net/inet/ip.h>
#include <proc/sched.h>
#include <uio.h>
#include <stderr.h>
#include <string.h>

/*
 * Receive/decode an ICMP packet.
 */
void icmp_receive(struct sk_buff *skb)
{
	skb->h.icmp_header = (struct icmp_header *) skb->data;
	skb_pull(skb, sizeof(struct icmp_header));

	/* recompute checksum */
	skb->h.icmp_header->chksum = 0;
	skb->h.icmp_header->chksum = net_checksum(skb->h.icmp_header, skb->size - sizeof(struct ethernet_header) - sizeof(struct ip_header));
}

/*
 * Reply to an ICMP echo request.
 */
void icmp_reply_echo(struct sk_buff *skb)
{
	struct sk_buff *skb_reply;
	uint16_t data_len, ip_len;

	/* compute data length */
	ip_len = ntohs(skb->nh.ip_header->length);
	data_len = ip_len - sizeof(struct ip_header) - sizeof(struct icmp_header);

	/* allocate reply */
	skb_reply = (struct sk_buff *) skb_alloc(sizeof(struct ethernet_header)
						 + sizeof(struct ip_header)
						 + sizeof(struct icmp_header)
						 + data_len);
	if (!skb_reply)
		return;

	/* build ethernet header */
	skb_reply->eth_header = (struct ethernet_header *) skb_put(skb_reply, sizeof(struct ethernet_header));
	ethernet_build_header(skb_reply->eth_header, skb->dev->mac_addr, skb->eth_header->src_mac_addr, ETHERNET_TYPE_IP);

	/* build ip header */
	skb_reply->nh.ip_header = (struct ip_header *) skb_put(skb_reply, sizeof(struct ip_header));
	ip_build_header(skb_reply->nh.ip_header, skb->nh.ip_header->tos, ip_len, ntohs(skb->nh.ip_header->id),
			skb->nh.ip_header->ttl, IP_PROTO_ICMP, skb->dev->ip_addr, skb->nh.ip_header->src_addr);

	/* copy icmp header */
	skb_reply->h.icmp_header = (struct icmp_header *) skb_put(skb_reply, sizeof(struct icmp_header) + data_len);
	memcpy(skb_reply->h.icmp_header, skb->h.icmp_header, sizeof(struct icmp_header) + data_len);

	/* set reply */
	skb_reply->h.icmp_header->type = 0;
	skb_reply->h.icmp_header->code = 0;
	skb_reply->h.icmp_header->chksum = 0;
	skb_reply->h.icmp_header->chksum = net_checksum(skb_reply->h.icmp_header, sizeof(struct icmp_header) + data_len);

	/* transmit reply */
	net_transmit(skb->dev, skb_reply);
}

/*
 * Handle an ICMP packet.
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
	list_add_tail(&skb_new->list, &sk->skb_list);

	return 0;
}

/*
 * Send an ICMP message.
 */
static int icmp_sendmsg(struct sock *sk, const struct msghdr *msg, int nonblock, int flags)
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
	skb = skb_alloc(sizeof(struct ethernet_header) + sizeof(struct ip_header) + len);
	if (!skb)
		return -ENOMEM;

	/* build ethernet header */
	skb->eth_header = (struct ethernet_header *) skb_put(skb, sizeof(struct ethernet_header));
	ethernet_build_header(skb->eth_header, sk->protinfo.af_inet.dev->mac_addr, NULL, ETHERNET_TYPE_IP);

	/* build ip header */
	skb->nh.ip_header = (struct ip_header *) skb_put(skb, sizeof(struct ip_header));
	ip_build_header(skb->nh.ip_header, 0, sizeof(struct ip_header) + len, 0, IPV4_DEFAULT_TTL, IP_PROTO_ICMP, sk->protinfo.af_inet.dev->ip_addr, dest_ip);

	/* copy icmp message */
	skb->h.icmp_header = buf = (struct icmp_header *) skb_put(skb, len);
	for (i = 0; i < msg->msg_iovlen; i++) {
		memcpy(buf, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
		buf += msg->msg_iov[i].iov_len;
	}

	/* recompute checksum */
	skb->h.icmp_header->chksum = 0;
	skb->h.icmp_header->chksum = net_checksum(skb->h.icmp_header, len);

	/* transmit message */
	net_transmit(sk->protinfo.af_inet.dev, skb);

	return len;
}

/*
 * Receive an ICMP message.
 */
static int icmp_recvmsg(struct sock *sk, struct msghdr *msg, int nonblock, int flags)
{
	size_t len, n, count = 0;
	struct sockaddr_in *sin;
	struct sk_buff *skb;
	void *buf;
	size_t i;

	/* unused flags */
	UNUSED(flags);

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
		sleep_on(&sk->sock->wait);
	}

	/* get first message */
	skb = list_first_entry(&sk->skb_list, struct sk_buff, list);

	/* get IP header */
	skb->nh.ip_header = (struct ip_header *) (skb->head + sizeof(struct ethernet_header));

	/* get ICMP header */
	skb->h.icmp_header = (struct icmp_header *) (skb->head + sizeof(struct ethernet_header) + sizeof(struct ip_header));

	/* get message */
	buf = skb->h.icmp_header + sk->msg_position;
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
		sin->sin_port = 0;
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
 * ICMP protocol.
 */
struct proto icmp_proto = {
	.handle		= icmp_handle,
	.recvmsg	= icmp_recvmsg,
	.sendmsg	= icmp_sendmsg,
};
