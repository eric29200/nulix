#include <net/sock.h>
#include <net/inet/ip.h>
#include <net/inet/net.h>
#include <drivers/net/rtl8139.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <fs/fs.h>
#include <stdio.h>
#include <stderr.h>
#include <math.h>

/* inet sockets */
static LIST_HEAD(inet_sockets);

static uint16_t dyn_port = 0;

/*
 * Deliver a packet to sockets.
 */
void net_deliver_skb(struct sk_buff_t *skb)
{
	struct list_head_t *pos;
	struct sock_t *sk;
	int ret;

	/* find matching sockets */
	list_for_each(pos, &inet_sockets) {
		sk = list_entry(pos, struct sock_t, list);

		/* handle packet */
		if (sk->protinfo.af_inet.prot && sk->protinfo.af_inet.prot->handle) {
			ret = sk->protinfo.af_inet.prot->handle(sk, skb);

			/* wake up waiting processes */
			if (ret == 0)
				task_wakeup_all(&sk->sock->wait);
		}
	}
}

/*
 * Get next free port.
 */
static uint16_t get_next_free_port()
{
	/* choose first dynamic port */
	if (!dyn_port) {
		dyn_port = IP_START_DYN_PORT + rand() % 4096;
		return dyn_port;
	}

	/* else get next free port */
	dyn_port++;
	return dyn_port;
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
	sk = kmalloc(sizeof(struct sock_t));
	if (!sk)
		return -ENOMEM;

	/* set socket */
	memset(sk, 0, sizeof(struct sock_t));
	sk->protinfo.af_inet.dev = rtl8139_get_net_device();
	sk->protocol = protocol;
	sk->sock = sock;
	sk->protinfo.af_inet.prot = prot;
	INIT_LIST_HEAD(&sk->skb_list);
	sock->sk = sk;

	/* insert in sockets list */
	list_add_tail(&sk->list, &inet_sockets);

	return 0;
}

/*
 * Duplicate a socket.
 */
static int inet_dup(struct socket_t *sock, struct socket_t *sock_new)
{
	struct sock_t *sk;

	/* get inet socket */
	sk = sock->sk;
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
	sk = sock->sk;
	if (!sk)
		return 0;

	/* free all remaining buffers */
	list_for_each_safe(pos, n, &sk->skb_list) {
		skb = list_entry(pos, struct sk_buff_t, list);
		list_del(&skb->list);
		skb_free(skb);
	}

	/* protocol close */
	if (sk->protinfo.af_inet.prot->close)
		sk->protinfo.af_inet.prot->close(sk);

	/* release inet socket */
	list_del(&sk->list);
	kfree(sk);

	return 0;
}

/*
 * Poll on a socket.
 */
static int inet_poll(struct socket_t *sock, struct select_table_t *wait)
{
	struct sock_t *sk;
	int mask = 0;

	/* get inet socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* check if there is a message in the queue */
	if (sk->sock->state == SS_DISCONNECTING || !list_empty(&sk->skb_list))
		mask |= POLLIN;

	/* check if socket can write */
	if (sk->sock->state != SS_DISCONNECTING && sk->sock->state != SS_DEAD)
		mask |= POLLOUT;

	/* add wait queue to select table */
	select_wait(&sock->wait, wait);

	return mask;
}

/*
 * Receive a message.
 */
static int inet_recvmsg(struct socket_t *sock, struct msghdr_t *msg, int nonblock, int flags)
{
	struct sock_t *sk;

	/* get inet socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* recvmsg not implemented */
	if (!sk->protinfo.af_inet.prot || !sk->protinfo.af_inet.prot->recvmsg)
		return -EINVAL;

	return sk->protinfo.af_inet.prot->recvmsg(sk, msg, nonblock, flags);
}

/*
 * Send a message.
 */
static int inet_sendmsg(struct socket_t *sock, const struct msghdr_t *msg, int nonblock, int flags)
{
	struct sock_t *sk;

	/* get inet socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* sendmsg not implemented */
	if (!sk->protinfo.af_inet.prot || !sk->protinfo.af_inet.prot->sendmsg)
		return -EINVAL;

	return sk->protinfo.af_inet.prot->sendmsg(sk, msg, nonblock, flags);
}

/*
 * Bind system call (attach an address to a socket).
 */
static int inet_bind(struct socket_t *sock, const struct sockaddr *addr, size_t addrlen)
{
	struct sockaddr_in *src_sin;
	struct sock_t *sk, *sk_tmp;
	struct list_head_t *pos;

	/* unused address length */
	UNUSED(addrlen);

	/* get inet socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* get internet address */
	src_sin = (struct sockaddr_in *) addr;

	/* check if asked port is already mapped */
	list_for_each(pos, &inet_sockets) {
		sk_tmp = list_entry(pos, struct sock_t, list);

		/* different protocol : skip */
		if (sk->protocol != sk_tmp->protocol)
			continue;

		/* already mapped */
		if (src_sin->sin_port && src_sin->sin_port == sk->protinfo.af_inet.src_sin.sin_port)
			return -ENOSPC;
	}

	/* set default address */
	if (!src_sin->sin_addr)
		src_sin->sin_addr = inet_iton(sk->protinfo.af_inet.dev->ip_addr);

	/* allocate a dynamic port */
	if (!src_sin->sin_port)
		src_sin->sin_port = htons(get_next_free_port());

	/* copy address */
	memcpy(&sk->protinfo.af_inet.src_sin, src_sin, sizeof(struct sockaddr));

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
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* get new inet socket */
	sk_new = sock_new->sk;
	if (!sk_new)
		return -EINVAL;

	/* accept not implemented */
	if (!sk_new->protinfo.af_inet.prot || !sk_new->protinfo.af_inet.prot->accept)
		return -EINVAL;

	/* accept */
	ret = sk_new->protinfo.af_inet.prot->accept(sk, sk_new);
	if (ret)
		return ret;

	/* set accepted address */
	if (addr)
		memcpy(addr, &sk_new->protinfo.af_inet.dst_sin, sizeof(struct sockaddr_in));

	return 0;
}

/*
 * Shutdown system call.
 */
static int inet_shutdown(struct socket_t *sock, int how)
{
	UNUSED(sock);
	UNUSED(how);

	printf("inet_shutdown() not implemented\n");

	return 0;
}

/*
 * Connect system call.
 */
static int inet_connect(struct socket_t *sock, const struct sockaddr *addr, size_t addrlen)
{
	struct sock_t *sk;

	/* unused address length */
	UNUSED(addrlen);

	/* get inet socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* allocate a dynamic port */
	sk->protinfo.af_inet.src_sin.sin_port = htons(get_next_free_port());

	/* copy address */
	memcpy(&sk->protinfo.af_inet.dst_sin, addr, sizeof(struct sockaddr));

	/* connect not implemented */
	if (!sk->protinfo.af_inet.prot || !sk->protinfo.af_inet.prot->connect)
		return -EINVAL;

	return sk->protinfo.af_inet.prot->connect(sk);
}

/*
 * Get peer name system call.
 */
static int inet_getpeername(struct socket_t *sock, struct sockaddr *addr, size_t *addrlen)
{
	struct sockaddr_in *sin;
	struct sock_t *sk;

	/* get inet socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* copy destination address */
	memset(addr, 0, sizeof(struct sockaddr));
	sin = (struct sockaddr_in *) addr;
	sin->sin_family = AF_INET;
	sin->sin_port = sk->protinfo.af_inet.dst_sin.sin_port;
	sin->sin_addr = sk->protinfo.af_inet.dst_sin.sin_addr;
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
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* copy destination address */
	memset(addr, 0, sizeof(struct sockaddr));
	sin = (struct sockaddr_in *) addr;
	sin->sin_family = AF_INET;
	sin->sin_port = sk->protinfo.af_inet.src_sin.sin_port;
	sin->sin_addr = sk->protinfo.af_inet.src_sin.sin_addr;
	*addrlen = sizeof(struct sockaddr_in);

	return 0;
}

/*
 * Get socket options system call.
 */
static int inet_getsockopt(struct socket_t *sock, int level, int optname, void *optval, size_t *optlen)
{
	UNUSED(sock);
	UNUSED(level);
	UNUSED(optname);
	UNUSED(optval);
	UNUSED(optlen);

	printf("inet_getsockopt(%d) undefined\n", optname);

	return 0;
}

/*
 * Set socket options system call.
 */
static int inet_setsockopt(struct socket_t *sock, int level, int optname, void *optval, size_t optlen)
{
	UNUSED(sock);
	UNUSED(level);
	UNUSED(optname);
	UNUSED(optval);
	UNUSED(optlen);

	printf("inet_setsockopt(%d) undefined\n", optname);

	return 0;
}

/*
 * Inet operations.
 */
struct prot_ops inet_ops = {
	.create		= inet_create,
	.dup		= inet_dup,
	.release	= inet_release,
	.poll		= inet_poll,
	.recvmsg	= inet_recvmsg,
	.sendmsg	= inet_sendmsg,
	.bind		= inet_bind,
	.accept		= inet_accept,
	.connect	= inet_connect,
	.shutdown	= inet_shutdown,
	.getpeername	= inet_getpeername,
	.getsockname	= inet_getsockname,
	.getsockopt	= inet_getsockopt,
	.setsockopt	= inet_setsockopt,
};
