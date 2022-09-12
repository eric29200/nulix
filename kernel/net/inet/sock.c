#include <net/inet/sock.h>
#include <net/inet/ip.h>
#include <net/inet/net.h>
#include <drivers/rtl8139.h>
#include <mm/mm.h>
#include <fs/fs.h>
#include <stderr.h>

static uint16_t next_port = IP_START_DYN_PORT;
extern struct socket_t sockets[NR_SOCKETS];

/*
 * Get next free port.
 */
static uint16_t get_next_free_port()
{
	uint16_t port = next_port;
	next_port++;
	return port;
}

/*
 * Create a socket.
 */
static int inet_create(struct socket_t *sock, int protocol)
{
	struct proto_t *prot = NULL;
	struct sock_t *sk;

	/* choose socket type */
	switch (sock->type) {
		case SOCK_STREAM:
			switch (protocol) {
				case 0:
				case IP_PROTO_TCP:
					protocol = IP_PROTO_TCP;
					prot = &tcp_proto;
					break;
				default:
					return -EINVAL;
			}

			break;
		case SOCK_DGRAM:
			switch (protocol) {
				case 0:
				case IP_PROTO_UDP:
					protocol = IP_PROTO_UDP;
					prot = &udp_proto;
					break;
				case IP_PROTO_ICMP:
					prot = &icmp_proto;
					break;
				default:
					return -EINVAL;
			}

			break;
		case SOCK_RAW:
			prot = &raw_proto;
			break;
		default:
			return -EINVAL;
	}

	/* allocate inet socket */
	sock->data = sk = kmalloc(sizeof(struct sock_t));
	if (!sock->data)
		return -ENOMEM;

	/* set socket */
	memset(sk, 0, sizeof(struct sock_t));
	sk->dev = rtl8139_get_net_device();
	sk->protocol = protocol;
	sk->sock = sock;
	sk->prot = prot;
	INIT_LIST_HEAD(&sk->skb_list);

	return 0;
}

/*
 * Duplicate a socket.
 */
static int inet_dup(struct socket_t *sock, struct socket_t *sock_new)
{
	struct sock_t *sk;

	/* get inet socket */
	sk = (struct sock_t *) sock->data;
	if (!sk)
		return 0;

	return inet_create(sock_new, sk->protocol);
}

/*
 * Release a socket.
 */
static int inet_release(struct socket_t *sock)
{
	struct list_head_t *pos, *n;
	struct sk_buff_t *skb;
	struct sock_t *sk;

	/* get inet socket */
	sk = (struct sock_t *) sock->data;
	if (!sk)
		return 0;

	/* free all remaining buffers */
	list_for_each_safe(pos, n, &sk->skb_list) {
		skb = list_entry(pos, struct sk_buff_t, list);
		list_del(&skb->list);
		skb_free(skb);
	}

	/* release inet socket */
	kfree(sock->data);

	return 0;
}

/*
 * Close a socket.
 */
static int inet_close(struct socket_t *sock)
{
	struct sock_t *sk;

	/* get inet socket */
	sk = (struct sock_t *) sock->data;
	if (!sk)
		return -EINVAL;

	/* close protocol operation */
	if (sk->prot && sk->prot->close)
		return sk->prot->close(sk);

	return 0;
}

/*
 * Poll on a socket.
 */
static int inet_poll(struct socket_t *sock)
{
	struct sock_t *sk;
	int mask = 0;

	/* get inet socket */
	sk = (struct sock_t *) sock->data;
	if (!sk)
		return -EINVAL;

	/* check if there is a message in the queue */
	if (!list_empty(&sk->skb_list))
		mask |= POLLIN;

	return mask;
}

/*
 * Receive a message.
 */
static int inet_recvmsg(struct socket_t *sock, struct msghdr_t *msg, int flags)
{
	struct sock_t *sk;

	/* get inet socket */
	sk = (struct sock_t *) sock->data;
	if (!sk)
		return -EINVAL;

	/* recvmsg not implemented */
	if (!sk->prot || !sk->prot->recvmsg)
		return -EINVAL;

	return sk->prot->recvmsg(sk, msg, flags);
}

/*
 * Send a message.
 */
static int inet_sendmsg(struct socket_t *sock, const struct msghdr_t *msg, int flags)
{
	struct sock_t *sk;

	/* get inet socket */
	sk = (struct sock_t *) sock->data;
	if (!sk)
		return -EINVAL;

	/* sendmsg not implemented */
	if (!sk->prot || !sk->prot->sendmsg)
		return -EINVAL;

	return sk->prot->sendmsg(sk, msg, flags);
}

/*
 * Bind system call (attach an address to a socket).
 */
static int inet_bind(struct socket_t *sock, const struct sockaddr *addr, size_t addrlen)
{
	struct sockaddr_in *src_sin;
	struct sock_t *sk, *sk_tmp;
	int i;

	/* unused address length */
	UNUSED(addrlen);

	/* get inet socket */
	sk = (struct sock_t *) sock->data;
	if (!sk)
		return -EINVAL;

	/* get internet address */
	src_sin = (struct sockaddr_in *) addr;

	/* check if asked port is already mapped */
	for (i = 0; i < NR_SOCKETS; i++) {
		/* different domain : skip */
		if (!sockets[i].state || sockets[i].family != AF_INET)
			continue;

		/* get inet socket */
		sk_tmp = (struct sock_t *) sockets[i].data;
		if (!sk_tmp)
			continue;

		/* different protocol : skip */
		if (sk->protocol != sk_tmp->protocol)
			continue;

		/* already mapped */
		if (src_sin->sin_port && src_sin->sin_port == sk->src_sin.sin_port)
			return -ENOSPC;
	}

	/* allocate a dynamic port */
	if (!src_sin->sin_port)
		src_sin->sin_port = htons(get_next_free_port());

	/* copy address */
	memcpy(&sk->src_sin, src_sin, sizeof(struct sockaddr));

	return 0;
}

/*
 * Accept system call.
 */
static int inet_accept(struct socket_t *sock, struct socket_t *sock_new, struct sockaddr *addr)
{
	struct sock_t *sk, *sk_new;
	int ret;

	/* get inet socket */
	sk = (struct sock_t *) sock->data;
	if (!sk)
		return -EINVAL;

	/* get new inet socket */
	sk_new = (struct sock_t *) sock_new->data;
	if (!sk_new)
		return -EINVAL;

	/* accept not implemented */
	if (!sk_new->prot || !sk_new->prot->accept)
		return -EINVAL;

	/* accept */
	ret = sk_new->prot->accept(sk, sk_new);
	if (ret)
		return ret;

	/* set accepted address */
	if (addr)
		memcpy(addr, &sk_new->dst_sin, sizeof(struct sockaddr_in));

	return 0;
}

/*
 * Connect system call.
 */
static int inet_connect(struct socket_t *sock, const struct sockaddr *addr)
{
	struct sock_t *sk;

	/* get inet socket */
	sk = (struct sock_t *) sock->data;
	if (!sk)
		return -EINVAL;

	/* allocate a dynamic port */
	sk->src_sin.sin_port = htons(get_next_free_port());

	/* copy address */
	memcpy(&sk->dst_sin, addr, sizeof(struct sockaddr));

	/* connect not implemented */
	if (!sk->prot || !sk->prot->connect)
		return -EINVAL;

	return sk->prot->connect(sk);
}

/*
 * Get peer name system call.
 */
static int inet_getpeername(struct socket_t *sock, struct sockaddr *addr, size_t *addrlen)
{
	struct sockaddr_in *sin;
	struct sock_t *sk;

	/* get inet socket */
	sk = (struct sock_t *) sock->data;
	if (!sk)
		return -EINVAL;

	/* copy destination address */
	memset(addr, 0, sizeof(struct sockaddr));
	sin = (struct sockaddr_in *) addr;
	sin->sin_family = AF_INET;
	sin->sin_port = sk->dst_sin.sin_port;
	sin->sin_addr = sk->dst_sin.sin_addr;
	*addrlen = sizeof(struct sockaddr_in);

	return 0;
}

/*
 * Get sock name system call.
 */
static int inet_getsockname(struct socket_t *sock, struct sockaddr *addr, size_t *addrlen)
{
	struct sockaddr_in *sin;
	struct sock_t *sk;

	/* get inet socket */
	sk = (struct sock_t *) sock->data;
	if (!sk)
		return -EINVAL;

	/* copy destination address */
	memset(addr, 0, sizeof(struct sockaddr));
	sin = (struct sockaddr_in *) addr;
	sin->sin_family = AF_INET;
	sin->sin_port = sk->src_sin.sin_port;
	sin->sin_addr = sk->src_sin.sin_addr;
	*addrlen = sizeof(struct sockaddr_in);

	return 0;
}

/*
 * Inet operations.
 */
struct prot_ops inet_ops = {
	.create		= inet_create,
	.dup		= inet_dup,
	.release	= inet_release,
	.close		= inet_close,
	.poll		= inet_poll,
	.recvmsg	= inet_recvmsg,
	.sendmsg	= inet_sendmsg,
	.bind		= inet_bind,
	.accept		= inet_accept,
	.connect	= inet_connect,
	.getpeername	= inet_getpeername,
	.getsockname	= inet_getsockname,
};
