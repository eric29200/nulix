#include <net/sock.h>
#include <net/inet/ip.h>
#include <net/inet/icmp.h>
#include <net/inet/net.h>
#include <net/inet/tcp.h>
#include <net/inet/route.h>
#include <net/if.h>
#include <fs/proc_fs.h>
#include <drivers/net/rtl8139.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <fs/fs.h>
#include <stdio.h>
#include <stderr.h>
#include <math.h>

/* inet sockets */
static LIST_HEAD(inet_sockets);

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
 * Insert a socket in the inet list (list must be sorted by protocol and source port).
 */
static void insert_inet_socket(struct sock *sk)
{
	struct list_head *pos;
	struct sock *sk_tmp;

	/* empty list */
	if (list_empty(&inet_sockets)) {
		list_add(&sk->list, &inet_sockets);
		return;
	}

	/* find insert position */
	list_for_each(pos, &inet_sockets) {
		sk_tmp = list_entry(pos, struct sock, list);
		if (sk_tmp->protocol > sk->protocol)
			break;
		if (sk_tmp->protocol == sk->protocol
			&& ntohs(sk_tmp->protinfo.af_inet.sport) >= ntohs(sk->protinfo.af_inet.sport))
			break;
	}

	/* insert socket */
	if (pos == &inet_sockets)
		list_add(&sk->list, &sk_tmp->list);
	else
		list_add_tail(&sk->list, &sk_tmp->list);
}

/*
 * Check if a port is already mapped.
 */
static int check_free_port(uint16_t protocol, uint16_t sport)
{
	struct list_head *pos;
	struct sock *sk_tmp;

	/* check if asked port is already mapped */
	list_for_each(pos, &inet_sockets) {
		sk_tmp = list_entry(pos, struct sock, list);

		if (sk_tmp->protocol < protocol)
			continue;
		else if (sk_tmp->protocol > protocol || ntohs(sk_tmp->protinfo.af_inet.sport) > sport)
			break;
		else if (ntohs(sk_tmp->protinfo.af_inet.sport) == sport)
			return 0;
	}

	return sport;
}

/*
 * Get a free port.
 */
static uint16_t get_free_port(uint16_t protocol)
{
	int base = IP_START_DYN_PORT;
	struct list_head *pos;
	struct sock *sk_tmp;

	list_for_each(pos, &inet_sockets) {
		sk_tmp = list_entry(pos, struct sock, list);

		if (sk_tmp->protocol < protocol)
			continue;
		else if (sk_tmp->protocol > protocol || sk_tmp->protinfo.af_inet.sport > base)
			break;
		else if (ntohs(sk_tmp->protinfo.af_inet.sport) == base)
			base++;
	}

	return base > USHRT_MAX ? 0 : base;
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
	struct sockaddr_in *addr_in = (struct sockaddr_in *) addr;
	struct sock *sk;
	uint32_t saddr;
	uint16_t sport;

	/* get inet socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* check active socket, bad address length and double bind */
	if (sk->state != TCP_CLOSE || addrlen < sizeof(struct sockaddr_in) || sk->protinfo.af_inet.sport)
		return -EINVAL;

	/* get internet address */
	saddr = addr_in->sin_addr;
	sport = addr_in->sin_port;

	/* set default address */
	if (!saddr)
		saddr = sk->protinfo.af_inet.dev->ip_addr;

	/* check or get a free port */
	if (sport)
		sport = check_free_port(sk->protocol, ntohs(sport));
	else
		sport = get_free_port(sk->protocol);

	/* no free port */
	if (!sport)
		return -EADDRINUSE;

	/* copy address */
	sk->protinfo.af_inet.saddr = saddr;
	sk->protinfo.af_inet.sport = htons(sport);

	/* reinsert socket */
	list_del(&sk->list);
	insert_inet_socket(sk);

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
	struct sockaddr_in *addr_in = (struct sockaddr_in *) addr;
	struct sock *sk;

	/* unused address length */
	UNUSED(addrlen);

	/* get inet socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* allocate a dynamic port */
	sk->protinfo.af_inet.sport = htons(get_free_port(sk->protocol));
	list_del(&sk->list);
	insert_inet_socket(sk);

	/* copy address */
	sk->protinfo.af_inet.daddr = addr_in->sin_addr;
	sk->protinfo.af_inet.dport = addr_in->sin_port;

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
	struct sockaddr_in *addr_in = (struct sockaddr_in *) addr;
	struct sock *sk;

	/* get inet socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* copy destination address */
	memset(addr, 0, sizeof(struct sockaddr));
	addr_in->sin_family = AF_INET;
	*addrlen = sizeof(struct sockaddr_in);

	if (peer) {
		addr_in->sin_port = sk->protinfo.af_inet.dport;
		addr_in->sin_addr = sk->protinfo.af_inet.daddr;
	} else {
		addr_in->sin_port = sk->protinfo.af_inet.sport;
		addr_in->sin_addr = sk->protinfo.af_inet.saddr;
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
 * INET device ioctl.
 */
static int devinet_ioctl(int cmd, unsigned long arg)
{
	struct ifreq *ifr = (struct ifreq *) arg;
	struct sockaddr_in *addr_in;
	struct net_device *dev;

	/* get network device */
	dev = net_device_find(ifr->ifr_ifrn.ifrn_name);
	if (!dev)
		return -EINVAL;

	/* get address */
	addr_in = (struct sockaddr_in *) &ifr->ifr_ifru.ifru_addr;

	/* check parameters */
	switch (cmd) {
		case SIOCGIFADDR:
		case SIOCGIFNETMASK:
			memset(addr_in, 0, sizeof(struct sockaddr_in));
		 	addr_in->sin_family = AF_INET;
			break;
		case SIOCSIFADDR:
		case SIOCSIFNETMASK:
			if (addr_in->sin_family != AF_INET)
				return -EINVAL;
			break;
		default:
			return -ENOIOCTLCMD;
	}

	/* do ioctl */
	switch (cmd) {
		case SIOCGIFADDR:
			addr_in->sin_addr = dev->ip_addr;
			return 0;
		case SIOCGIFNETMASK:
		 	addr_in->sin_addr = dev->ip_netmask;
			return 0;
		case SIOCSIFADDR:
			dev->ip_addr = addr_in->sin_addr;
			return 0;
		case SIOCSIFNETMASK:
			dev->ip_netmask = addr_in->sin_addr;
			return 0;
		default:
			return -ENOIOCTLCMD;
	}
}

/*
 * Ioctl on a INET socket.
 */
static int inet_ioctl(struct socket *sock, int cmd, unsigned long arg)
{
	/* unused socket */
	UNUSED(sock);

	switch (cmd) {
		case SIOCGIFADDR:
		case SIOCGIFNETMASK:
		case SIOCSIFADDR:
		case SIOCSIFNETMASK:
			return devinet_ioctl(cmd, arg);
		case SIOCADDRT:
			return ip_rt_ioctl(cmd, (void *) arg);
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
					prot = &tcp_prot;
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
					prot = &udp_prot;
					break;
				default:
					return -EINVAL;
			}

			break;
		case SOCK_RAW:
			prot = &raw_prot;
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
	sk->protinfo.af_inet.dev = rtl8139_get_device();
	sk->protocol = protocol;
	sk->protinfo.af_inet.prot = prot;
	sock->ops = &inet_ops;

	/* init data */
	sock_init_data(sock, sk);

	/* insert in sockets list */
	insert_inet_socket(sk);

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
	icmp_init(&inet_family_ops);

	/* register net/route procfs entry */
	create_proc_read_entry("route", 0, proc_net, route_read_proc);
}
