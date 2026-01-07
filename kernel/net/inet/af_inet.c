#include <net/sock.h>
#include <net/inet/ip.h>
#include <net/inet/net.h>
#include <net/if.h>
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

static int inet_create(struct socket *sock, int protocol);

/*
 * Deliver a packet to sockets.
 */
void net_deliver_skb(struct sk_buff *skb)
{
	struct list_head *pos;
	struct sock *sk;
	int ret;

	/* find matching sockets */
	list_for_each(pos, &inet_sockets) {
		sk = list_entry(pos, struct sock, list);

		/* handle packet */
		if (sk->protinfo.af_inet.prot && sk->protinfo.af_inet.prot->handle) {
			ret = sk->protinfo.af_inet.prot->handle(sk, skb);

			/* wake up waiting processes */
			if (ret == 0)
				wake_up(&sk->socket->wait);
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
 * Duplicate a socket.
 */
static int inet_dup(struct socket *sock_new, struct socket *sock)
{
	struct sock *sk;

	/* get inet socket */
	sk = sock->sk;
	if (!sk)
		return 0;

	return inet_create(sock_new, sk->protocol);
}

/*
 * Release a socket.
 */
static int inet_release(struct socket *sock)
{
	struct sk_buff *skb;
	struct sock *sk;

	/* get inet socket */
	sk = sock->sk;
	if (!sk)
		return 0;

	/* free all remaining buffers */
	while ((skb = skb_dequeue(&sk->receive_queue)) != NULL)
		skb_free(skb);

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
static int inet_poll(struct socket *sock, struct select_table *wait)
{
	struct sock *sk;
	int mask = 0;

	/* get inet socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* connecting = waiting for TCP syn/ack */
	if (sk->socket->state == SS_CONNECTING)
		return mask;

	/* check if there is a message in the queue */
	if (sk->socket->state == SS_DISCONNECTING || !skb_queue_empty(&sk->receive_queue))
		mask |= POLLIN;

	/* check if socket can write */
	if (sk->socket->state != SS_DISCONNECTING && sk->socket->state != SS_DEAD)
		mask |= POLLOUT;

	/* add wait queue to select table */
	select_wait(&sock->wait, wait);

	return mask;
}

/*
 * Receive a message.
 */
static int inet_recvmsg(struct socket *sock, struct msghdr *msg, size_t len)
{
	struct sock *sk;

	/* get inet socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* recvmsg not implemented */
	if (!sk->protinfo.af_inet.prot || !sk->protinfo.af_inet.prot->recvmsg)
		return -EINVAL;

	return sk->protinfo.af_inet.prot->recvmsg(sk, msg, len);
}

/*
 * Send a message.
 */
static int inet_sendmsg(struct socket *sock, const struct msghdr *msg, size_t len)
{
	struct sock *sk;

	/* get inet socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* sendmsg not implemented */
	if (!sk->protinfo.af_inet.prot || !sk->protinfo.af_inet.prot->sendmsg)
		return -EINVAL;

	return sk->protinfo.af_inet.prot->sendmsg(sk, msg, len);
}

/*
 * Bind system call (attach an address to a socket).
 */
static int inet_bind(struct socket *sock, const struct sockaddr *addr, size_t addrlen)
{
	struct sockaddr_in *src_sin;
	struct sock *sk, *sk_tmp;
	struct list_head *pos;

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
		sk_tmp = list_entry(pos, struct sock, list);

		/* different protocol : skip */
		if (sk->protocol != sk_tmp->protocol)
			continue;

		/* already mapped */
		if (src_sin->sin_port && src_sin->sin_port == sk->protinfo.af_inet.src_sin.sin_port)
			return -ENOSPC;
	}

	/* set default address */
	if (!src_sin->sin_addr)
		src_sin->sin_addr = sk->protinfo.af_inet.dev->ip_addr;

	/* allocate a dynamic port */
	if (!src_sin->sin_port)
		src_sin->sin_port = htons(get_next_free_port());

	/* copy address */
	memcpy(&sk->protinfo.af_inet.src_sin, src_sin, sizeof(struct sockaddr));

	return 0;
}

/*
 * Listen system call.
 */
static int inet_listen(struct socket *sock)
{
	/* set socket state */
	sock->state = SS_LISTENING;

	return 0;
}

/*
 * Accept system call.
 */
static int inet_accept(struct socket *sock, struct socket *sock_new, int flags)
{
	struct sock *sk, *sk_new;
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
	ret = sk_new->protinfo.af_inet.prot->accept(sk, sk_new, flags);
	if (ret)
		return ret;

	return 0;
}

/*
 * Shutdown system call.
 */
static int inet_shutdown(struct socket *sock, int how)
{
	UNUSED(sock);
	UNUSED(how);

	printf("inet_shutdown() not implemented\n");

	return 0;
}

/*
 * Connect system call.
 */
static int inet_connect(struct socket *sock, const struct sockaddr *addr, size_t addrlen)
{
	struct sock *sk;

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
 * Get socket name.
 */
static int inet_getname(struct socket *sock, struct sockaddr *addr, size_t *addrlen, int peer)
{
	struct sockaddr_in *sin = (struct sockaddr_in *) addr;
	struct sock *sk;

	/* get inet socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* copy destination address */
	memset(addr, 0, sizeof(struct sockaddr));
	sin->sin_family = AF_INET;
	*addrlen = sizeof(struct sockaddr_in);

	if (peer) {
		sin->sin_port = sk->protinfo.af_inet.dst_sin.sin_port;
		sin->sin_addr = sk->protinfo.af_inet.dst_sin.sin_addr;
	} else {
		sin->sin_port = sk->protinfo.af_inet.src_sin.sin_port;
		sin->sin_addr = sk->protinfo.af_inet.src_sin.sin_addr;
	}

	return 0;
}

/*
 * Get socket options system call.
 */
static int inet_getsockopt(struct socket *sock, int level, int optname, void *optval, size_t *optlen)
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
static int inet_setsockopt(struct socket *sock, int level, int optname, void *optval, size_t optlen)
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
 * Ioctl on a INET socket.
 */
static int inet_ioctl(struct socket *sock, int cmd, unsigned long arg)
{
	struct ifreq *ifr = (struct ifreq *) arg;
	struct net_device *net_dev;
	struct sockaddr_in *sin;

	/* unused socket */
	UNUSED(sock);

	/* get network device */
	net_dev = net_device_find(ifr->ifr_ifrn.ifrn_name);
	if (!net_dev)
		return -EINVAL;

	/* get address */
	sin = (struct sockaddr_in *) &ifr->ifr_ifru.ifru_addr;

	/* check parameters */
	switch (cmd) {
		case SIOCGIFADDR:
		case SIOCGIFNETMASK:
			memset(sin, 0, sizeof(struct sockaddr_in));
		 	sin->sin_family = AF_INET;
			break;
		case SIOCSIFADDR:
		case SIOCSIFNETMASK:
			if (sin->sin_family != AF_INET)
				return -EINVAL;
			break;
		default:
			return -ENOIOCTLCMD;
	}

	/* do ioctl */
	switch (cmd) {
		case SIOCGIFADDR:
			sin->sin_addr = net_dev->ip_addr;
			return 0;
		case SIOCGIFNETMASK:
		 	sin->sin_addr = net_dev->ip_netmask;
			return 0;
		case SIOCSIFADDR:
			net_dev->ip_addr = sin->sin_addr;
			return 0;
		case SIOCSIFNETMASK:
			net_dev->ip_netmask = sin->sin_addr;
			return 0;
		default:
			return -ENOIOCTLCMD;
	}
}

/*
 * Inet operations.
 */
struct proto_ops inet_ops = {
	.dup		= inet_dup,
	.release	= inet_release,
	.poll		= inet_poll,
	.ioctl		= inet_ioctl,
	.recvmsg	= inet_recvmsg,
	.sendmsg	= inet_sendmsg,
	.bind		= inet_bind,
	.listen		= inet_listen,
	.accept		= inet_accept,
	.connect	= inet_connect,
	.shutdown	= inet_shutdown,
	.getname	= inet_getname,
	.getsockopt	= inet_getsockopt,
	.setsockopt	= inet_setsockopt,
};

/*
 * Create a socket.
 */
static int inet_create(struct socket *sock, int protocol)
{
	struct proto *prot = NULL;
	struct sock *sk;

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
	sk = kmalloc(sizeof(struct sock));
	if (!sk)
		return -ENOMEM;

	/* set socket */
	memset(sk, 0, sizeof(struct sock));
	sk->protinfo.af_inet.dev = rtl8139_get_net_device();
	sk->protocol = protocol;
	sk->protinfo.af_inet.prot = prot;
	sock->ops = &inet_ops;

	/* init data */
	sock_init_data(sock, sk);

	/* insert in sockets list */
	list_add_tail(&sk->list, &inet_sockets);

	return 0;
}

/*
 * UNIX protocol.
 */
static struct net_proto_family inet_family_ops = {
	.family		= PF_INET,
	.create		= inet_create,
};

/*
 * Init inet protocol.
 */
void inet_proto_init()
{
	sock_register(&inet_family_ops);
}
