#include <net/sk_buff.h>
#include <net/inet/sock.h>
#include <net/inet/net.h>
#include <net/inet/ethernet.h>
#include <net/inet/ip.h>
#include <proc/sched.h>
#include <uio.h>
#include <stderr.h>

/*
 * Handle a raw packet.
 */
static int raw_handle(struct sock_t *sk, struct sk_buff_t *skb)
{
	struct sk_buff_t *skb_new;

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
 * Receive a rax message.
 */
static int raw_recvmsg(struct sock_t *sk, struct msghdr_t *msg, int flags)
{
	size_t len, n, count = 0, i;
	struct sockaddr_in *sin;
	struct sk_buff_t *skb;

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
		task_sleep(&sk->sock->wait);
	}

	/* get first message */
	skb = list_first_entry(&sk->skb_list, struct sk_buff_t, list);

	/* get IP header */
	skb->nh.ip_header = (struct ip_header_t *) (skb->head + sizeof(struct ethernet_header_t));

	/* compute message length */
	len = skb->size - sizeof(struct ethernet_header_t);

	/* copy message */
	for (i = 0; i < msg->msg_iovlen; i++) {
		n = len > msg->msg_iov[i].iov_len ? msg->msg_iov[i].iov_len : len;
		memcpy(msg->msg_iov[i].iov_base, skb->nh.ip_header, n);
		count += n;
	}

	/* set source address */
	sin = (struct sockaddr_in *) msg->msg_name;
	sin->sin_family = AF_INET;
	sin->sin_port = 0;
	sin->sin_addr = inet_iton(skb->nh.ip_header->src_addr);

	/* remove and free socket buffer */
	list_del(&skb->list);
	skb_free(skb);

	return count;
}

/*
 * Send a raw message.
 */
static int raw_sendmsg(struct sock_t *sk, const struct msghdr_t *msg, int flags)
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
	skb = skb_alloc(sizeof(struct ethernet_header_t) + sizeof(struct ip_header_t) + len);
	if (!skb)
		return -ENOMEM;

	/* build ethernet header */
	skb->eth_header = (struct ethernet_header_t *) skb_put(skb, sizeof(struct ethernet_header_t));
	ethernet_build_header(skb->eth_header, sk->dev->mac_addr, NULL, ETHERNET_TYPE_IP);

	/* build ip header */
	skb->nh.ip_header = (struct ip_header_t *) skb_put(skb, sizeof(struct ip_header_t));
	ip_build_header(skb->nh.ip_header, 0, sizeof(struct ip_header_t) + len, 0, IPV4_DEFAULT_TTL, sk->protocol, sk->dev->ip_addr, dest_ip);

	/* copy ip message */
	buf = (struct ip_header_t *) skb_put(skb, len);
	for (i = 0; i < msg->msg_iovlen; i++) {
		memcpy(buf, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
		buf += msg->msg_iov[i].iov_len;
	}

	/* send message */
	net_transmit(sk->dev, skb);

	return len;
}

/*
 * Raw protocol.
 */
struct proto_t raw_proto = {
	.handle		= raw_handle,
	.recvmsg	= raw_recvmsg,
	.sendmsg	= raw_sendmsg,
};
