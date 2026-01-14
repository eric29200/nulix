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
		if (sk->prot && sk->prot->handle) {
			ret = sk->prot->handle(sk, skb);

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
			&& ntohs(sk_tmp->sport) >= ntohs(sk->sport))
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
		else if (sk_tmp->protocol > protocol || ntohs(sk_tmp->sport) > sport)
			break;
		else if (ntohs(sk_tmp->sport) == sport)
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
		else if (sk_tmp->protocol > protocol || sk_tmp->sport > base)
			break;
		else if (ntohs(sk_tmp->sport) == base)
			base++;
	}

	return base > USHRT_MAX ? 0 : base;
}

/*
 * Auto bind a socket.
 */
static int inet_autobind(struct sock *sk)
{
	/* already bound */
	if (sk->num)
		return 0;

	/* get a free port */
	sk->num = get_free_port(sk->protocol);
	if (!sk->num)
		return -EAGAIN;

	/* update socket */
	sk->sport = ntohs(sk->num);
	list_del(&sk->list);
	insert_inet_socket(sk);

	return 0;
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
	if (sk->prot->close)
		sk->prot->close(sk);

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
	struct sock *sk = sock->sk;

	/* check socket */
	if (!sk)
		return -EINVAL;

	/* poll not implemented */
	if (!sk->prot || !sk->prot->poll)
		return -EOPNOTSUPP;

	return sk->prot->poll(sk, wait);
}

/*
 * Receive a message.
 */
static int inet_recvmsg(struct socket *sock, struct msghdr *msg, size_t len)
{
	struct sock *sk = sock->sk;
	int ret;

	/* check socket */
	if (!sk)
		return -EINVAL;
	if (sk->err)
		return sock_error(sk);

	/* recvmsg not implemented */
	if (!sk->prot || !sk->prot->recvmsg)
		return -EOPNOTSUPP;

	/* autobind socket if needed */
	ret = inet_autobind(sk);
	if (ret)
		return ret;

	return sk->prot->recvmsg(sk, msg, len);
}

/*
 * Send a message.
 */
static int inet_sendmsg(struct socket *sock, const struct msghdr *msg, size_t len)
{
	struct sock *sk = sock->sk;
	int ret;

	/* check socket */
	if (!sk)
		return -EINVAL;
	if (sk->shutdown & SEND_SHUTDOWN) {
		send_sig(current_task, SIGPIPE, 1);
		return -EPIPE;
	}
	if (sk->err)
		return sock_error(sk);

	/* sendmsg not implemented */
	if (!sk->prot || !sk->prot->sendmsg)
		return -EOPNOTSUPP;

	/* autobind socket if needed */
	ret = inet_autobind(sk);
	if (ret)
		return ret;

	return sk->prot->sendmsg(sk, msg, len);
}

/*
 * Bind system call (attach an address to a socket).
 */
static int inet_bind(struct socket *sock, const struct sockaddr *addr, size_t addrlen)
{
	struct sockaddr_in *addr_in = (struct sockaddr_in *) addr;
	uint16_t snum = 0;
	int chk_addr_ret;
	struct sock *sk;

	/* get inet socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* check active socket and bad address length */
	if (sk->state != TCP_CLOSE || addrlen < sizeof(struct sockaddr_in))
		return -EINVAL;

	/* bind socket */
	if (sock->type != SOCK_RAW) {
		/* already bound */
		if (sk->num)
			return -EINVAL;

		/* check or get a free port */
		snum = ntohs(addr_in->sin_port);
		if (snum)
			snum = check_free_port(sk->protocol, snum);
		else
			snum = get_free_port(sk->protocol);

		/* no free port */
		if (!snum)
			return -EADDRINUSE;
	}

	/* check source address (must be ours) */
	chk_addr_ret = ip_chk_addr(addr_in->sin_addr);
	if (addr_in->sin_addr && chk_addr_ret != IS_MYADDR && chk_addr_ret != IS_BROADCAST)
		return -EADDRNOTAVAIL;

	/* set socket source address */
	if (chk_addr_ret || addr_in->sin_addr == 0) {
		sk->rcv_saddr = addr_in->sin_addr;
		sk->saddr = chk_addr_ret == IS_BROADCAST ? 0 : addr_in->sin_addr;
	}

	/* update socket */
	sk->num = snum;
	sk->sport = htons(snum);
	sk->daddr = 0;
	sk->dport = 0;

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
	UNUSED(sock);
	printf("inet_listen() not implemented\n");
	return -EINVAL;
}

/*
 * Accept system call.
 */
static int inet_accept(struct socket *sock, struct socket *sock_new, int flags)
{
	UNUSED(sock);
	UNUSED(sock_new);
	UNUSED(flags);
	printf("inet_accept() not implemented\n");
	return -EINVAL;
}

/*
 * Shutdown system call.
 */
static int inet_shutdown(struct socket *sock, int how)
{
	UNUSED(sock);
	UNUSED(how);
	printf("inet_shutdown() not implemented\n");
	return -EINVAL;
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
	sk->sport = htons(get_free_port(sk->protocol));
	list_del(&sk->list);
	insert_inet_socket(sk);

	/* copy address */
	sk->daddr = addr_in->sin_addr;
	sk->dport = addr_in->sin_port;

	/* connect not implemented */
	if (!sk->prot || !sk->prot->connect)
		return -EINVAL;

	return sk->prot->connect(sk);
}

/*
 * Get socket name.
 */
static int inet_getname(struct socket *sock, struct sockaddr *addr, size_t *addrlen, int peer)
{
	struct sockaddr_in *addr_in = (struct sockaddr_in *) addr;
	struct sock *sk;
	uint32_t saddr;

	/* get inet socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* copy destination address */
	memset(addr, 0, sizeof(struct sockaddr));
	addr_in->sin_family = AF_INET;
	*addrlen = sizeof(struct sockaddr_in);

	if (peer) {
		addr_in->sin_port = sk->dport;
		addr_in->sin_addr = sk->daddr;
	} else {
		saddr = sk->rcv_saddr;
		if (!saddr) {
			saddr = sk->saddr;
			if (!saddr)
				saddr = ip_my_addr();
		}

		addr_in->sin_port = sk->sport;
		addr_in->sin_addr = saddr;
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
	sk->protocol = protocol;
	sk->prot = prot;
	sock->ops = &inet_ops;

	/* raw sockets cannot be bound */
	if (sock->type == SOCK_RAW)
		sk->num = protocol;

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
