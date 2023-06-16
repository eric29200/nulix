#include <net/unix/af_unix.h>
#include <net/sk_buff.h>
#include <mm/mm.h>
#include <fs/fs.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>
#include <fcntl.h>
#include <uio.h>

/* UNIX sockets */
static LIST_HEAD(unix_sockets);

/*
 * Find a UNIX socket.
 */
static unix_socket_t *unix_find_socket_by_inode(struct inode_t *inode)
{
	struct list_head_t *pos;
	unix_socket_t *sk;

	list_for_each(pos, &unix_sockets) {
		sk = list_entry(pos, unix_socket_t, list);
		if (sk && sk->protinfo.af_unix.inode == inode)
			return sk;
	}

	return NULL;
}

/*
 * Find a UNIX socket.
 */
static unix_socket_t *unix_find_socket_by_name(struct sockaddr_un *sunaddr, size_t addrlen)
{
	struct list_head_t *pos;
	unix_socket_t *sk;

	list_for_each(pos, &unix_sockets) {
		sk = list_entry(pos, unix_socket_t, list);
		if (sk && sk->protinfo.af_unix.sunaddr_len == addrlen && memcmp(&sk->protinfo.af_unix.sunaddr, sunaddr, addrlen) == 0)
			return sk;
	}

	return NULL;
}

/*
 * Find other UNIX socket.
 */
static int unix_find_other(struct sockaddr_un *sunaddr, size_t addrlen, unix_socket_t **res)
{
	struct inode_t *inode;
	int ret = 0;

	/* reset result socket */
	*res = NULL;

	/* abstract socket */
	if (!sunaddr->sun_path[0]) {
		*res = unix_find_socket_by_name(sunaddr, addrlen);
		if (!*res)
			ret = -ECONNREFUSED;
	} else {
		/* find socket inode */
		ret = open_namei(AT_FDCWD, NULL, sunaddr->sun_path, S_IFSOCK, 0, &inode);
		if (ret)
			return ret;

		/* find UNIX socket */
		*res = unix_find_socket_by_inode(inode);
		if (!*res)
			ret = -ECONNREFUSED;

		/* release inode */
		iput(inode);
	}

	return ret;
}

/*
 * Create a socket.
 */
static int unix_create(struct socket_t *sock, int protocol)
{
	unix_socket_t *sk;

	/* check protocol */
	if (protocol != 0)
		return -EINVAL;

	/* allocate UNIX socket */
	sock->data = sk = (unix_socket_t *) kmalloc(sizeof(unix_socket_t));
	if (!sk)
		return -ENOMEM;

	/* set UNIX socket */
	memset(sk, 0, sizeof(unix_socket_t));
	sk->protocol = protocol;
	sk->sock = sock;
	sk->protinfo.af_unix.other = NULL;
	INIT_LIST_HEAD(&sk->list);
	INIT_LIST_HEAD(&sk->skb_list);

	/* insert UNIX socket */
	list_add(&sk->list, &unix_sockets);

	return 0;
}

/*
 * Duplicate a socket.
 */
static int unix_dup(struct socket_t *sock, struct socket_t *sock_new)
{
	unix_socket_t *sk;

	/* get UNIX socket */
	sk = (unix_socket_t *) sock->data;
	if (!sk)
		return -EINVAL;

	return unix_create(sock_new, sk->protocol);
}

/*
 * Release a socket.
 */
static int unix_release(struct socket_t *sock)
{
	unix_socket_t *sk;

	/* get UNIX socket */
	sk = (unix_socket_t *) sock->data;
	if (!sk)
		return 0;

	/* release inode */
	if (sk->protinfo.af_unix.inode)
		iput(sk->protinfo.af_unix.inode);

	/* remove UNIX socket */
	list_del(&sk->list);

	/* release UNIX socket */
	kfree(sock->data);

	return 0;
}

/*
 * Poll on a socket.
 */
static int unix_poll(struct socket_t *sock, struct select_table_t *wait)
{
	unix_socket_t *sk;
	int mask = 0;

	/* get inet socket */
	sk = (unix_socket_t *) sock->data;
	if (!sk)
		return -EINVAL;

	/* check if there is a message in the queue */
	if (!list_empty(&sk->skb_list) || sk->protinfo.af_unix.shutdown & RCV_SHUTDOWN)
		mask |= POLLIN;

	/* check if socket can write */
	if (sk->sock->state != SS_DEAD || sk->protinfo.af_unix.shutdown & SEND_SHUTDOWN)
		mask |= POLLOUT;

	/* add wait queue to select table */
	select_wait(&sock->wait, wait);

	return mask;
}

/*
 * Receive a message.
 */
static int unix_recvmsg(struct socket_t *sock, struct msghdr_t *msg, int nonblock, int flags)
{
	unix_socket_t *sk, *from;
	size_t n, i, len, count = 0;
	struct sk_buff_t *skb;
	void *buf;

	/* unused flags */
	UNUSED(flags);

	/* get UNIX socket */
	sk = (unix_socket_t *) sock->data;
	if (!sk)
		return -EINVAL;

	/* sleep until we receive a packet */
	for (;;) {
		/* signal received : restart system call */
		if (signal_pending(current_task))
			return -ERESTARTSYS;

		/* message received : break */
		if (!list_empty(&sk->skb_list))
			break;

		/* socket is down */
		if (sk->protinfo.af_unix.shutdown & RCV_SHUTDOWN)
			return 0;

		/* non blocking */
		if (nonblock)
			return -EAGAIN;

		/* sleep */
		task_sleep(&sk->sock->wait);
	}

	/* get first message */
	skb = list_first_entry(&sk->skb_list, struct sk_buff_t, list);

	/* get message */
	buf = skb->head + sk->msg_position;
	len = skb->len - sk->msg_position;

	/* copy message */
	for (i = 0; i < msg->msg_iovlen; i++) {
		n = len > msg->msg_iov[i].iov_len ? msg->msg_iov[i].iov_len : len;
		memcpy(msg->msg_iov[i].iov_base, buf, n);
		count += n;
		len -= n;
		buf += n;
	}

	/* set source address */
	if (skb->sock && skb->sock->data) {
		from = (unix_socket_t *) skb->sock->data;
		memcpy(msg->msg_name, &from->protinfo.af_unix.sunaddr, from->protinfo.af_unix.sunaddr_len);
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
 * Send a message.
 */
static int unix_sendmsg(struct socket_t *sock, const struct msghdr_t *msg, int nonblock, int flags)
{
	struct sockaddr_un *sunaddr = msg->msg_name;
	unix_socket_t *sk, *other;
	struct sk_buff_t *skb;
	size_t len, i;
	void *buf;
	int ret;

	/* unused flags */
	UNUSED(flags);
	UNUSED(nonblock);

	/* get UNIX socket */
	sk = (unix_socket_t *) sock->data;
	if (!sk)
		return -EINVAL;

	/* find other socket */
	if (sunaddr) {
		ret = unix_find_other(sunaddr, msg->msg_namelen, &other);
		if (ret)
			return ret;
	} else {
		other = sk->protinfo.af_unix.other;
		if (!other)
			return -ENOTCONN;
	}

	/* compute data length */
	len = 0;
	for (i = 0; i < msg->msg_iovlen; i++)
		len += msg->msg_iov[i].iov_len;

	/* allocate a socket buffer */
	skb = skb_alloc(len);
	if (!skb)
		return -ENOMEM;

	/* set socket */
	skb->sock = sock;

	/* copy message */
	buf = skb_put(skb, len);
	for (i = 0; i < msg->msg_iovlen; i++) {
		memcpy(buf, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
		buf += msg->msg_iov[i].iov_len;
	}

	/* queue message to other socket */
	list_add_tail(&skb->list, &other->skb_list);

	/* wake up eventual readers */
	task_wakeup_all(&other->sock->wait);

	return len;
}

/*
 * Bind system call (attach an address to a socket).
 */
static int unix_bind(struct socket_t *sock, const struct sockaddr *addr, size_t addrlen)
{
	struct sockaddr_un *sunaddr = (struct sockaddr_un *) addr;
	unix_socket_t *sk;
	int ret;

	/* check address length */
	if (addrlen < UN_PATH_OFFSET || addrlen > sizeof(struct sockaddr_un))
		return -EINVAL;

	/* get UNIX socket */
	sk = (unix_socket_t *) sock->data;
	if (!sk)
		return -EINVAL;

	/* socket already bound */
	if (sk->protinfo.af_unix.sunaddr_len || sk->protinfo.af_unix.inode)
		return -EBUSY;

	/* abstract socket */
	if (!sunaddr->sun_path[0]) {
		/* check if socket already exists */
		if (unix_find_socket_by_name(sunaddr, addrlen) != NULL)
			return -EADDRINUSE;
	} else {
		/* create socket file */
		ret = sys_mknod(sunaddr->sun_path, S_IFSOCK | S_IRWXUGO, 0);
		if (ret)
			return ret;

		/* get inode */
		ret = open_namei(AT_FDCWD, NULL, sunaddr->sun_path, 0, S_IFSOCK, &sk->protinfo.af_unix.inode);
		if (ret)
			return ret;
	}

	/* set socket address */
	memcpy(&sk->protinfo.af_unix.sunaddr, addr, addrlen);
	sk->protinfo.af_unix.sunaddr.sun_path[addrlen - UN_PATH_OFFSET] = 0;
	sk->protinfo.af_unix.sunaddr_len = addrlen;

	return 0;
}

/*
 * Accept system call.
 */
static int unix_accept(struct socket_t *sock, struct socket_t *sock_new, struct sockaddr *addr)
{
	unix_socket_t *sk, *sk_new, *other;
	struct list_head_t *pos;
	struct sk_buff_t *skb;

	/* socket must be listening */
	if (sock->type != SOCK_STREAM || sock->state != SS_LISTENING)
		return -EINVAL;

	/* get UNIX sockets */
	sk = (unix_socket_t *) sock->data;
	sk_new = (unix_socket_t *) sock_new->data;
	if (!sk || !sk_new)
		return -EINVAL;

	for (;;) {
		/* signal received : restart system call */
		if (signal_pending(current_task))
			return -ERESTARTSYS;

		/* for each received packet */
		list_for_each(pos, &sk->skb_list) {
			/* get socket buffer */
			skb = list_entry(pos, struct sk_buff_t, list);

			/* set new socket */
			sock_new->state = SS_CONNECTED;
			other = sk_new->protinfo.af_unix.other = (unix_socket_t *) skb->sock->data;
			sk_new->protinfo.af_unix.other = sk_new;

			/* set destination address */
			memcpy(addr, &other->protinfo.af_unix.sunaddr, sizeof(struct sockaddr_un));

			/* free socket buffer */
			list_del(&skb->list);
			skb_free(skb);

			/* set other socket connected and wake up eventual clients connecting */
			sk_new->protinfo.af_unix.other->sock->state = SS_CONNECTED;
			task_wakeup_all(&sk_new->protinfo.af_unix.other->sock->wait);

			return 0;
		}

		/* disconnected : break */
		if (sock->state == SS_DISCONNECTING)
			return 0;

		/* sleep */
		task_sleep(&sock->wait);
	}

	return 0;
}

/*
 * Connect system call.
 */
static int unix_connect(struct socket_t *sock, const struct sockaddr *addr, size_t addrlen)
{
	struct sockaddr_un *sunaddr = (struct sockaddr_un *) addr;
	unix_socket_t *sk, *other;
	struct sk_buff_t *skb;
	int ret;

	/* get UNIX socket */
	sk = (unix_socket_t *) sock->data;
	if (!sk)
		return -EINVAL;

	/* find other unix socket */
	ret = unix_find_other(sunaddr, addrlen, &other);
	if (ret)
		return ret;

	/* datagramm socket : just connect */
	if (sock->type == SOCK_DGRAM) {
		sk->protinfo.af_unix.other = other;
		sock->state = SS_CONNECTED;
		return 0;
	}

	/* create an empty packet */
	skb = skb_alloc(0);
	if (!skb)
		return -ENOMEM;

	/* queue empty packet */
	skb->sock = sock;
	list_add_tail(&skb->list, &other->skb_list);

	/* wake up eventual reader */
	task_wakeup_all(&other->sock->wait);

	/* set socket */
	sk->protinfo.af_unix.other = other;
	sock->state = SS_CONNECTING;

	/* wait for an accept */
	while (sock->state == SS_CONNECTING) {
		/* signal received : restart system call */
		if (signal_pending(current_task))
			return -ERESTARTSYS;

		/* sleep */
		task_sleep(&sock->wait);
	}

	return 0;
}

/*
 * Shutdown system call.
 */
static int unix_shutdown(struct socket_t *sock, int how)
{
	unix_socket_t *sk, *other;

	/* get UNIX socket */
	sk = (unix_socket_t *) sock->data;
	if (!sk)
		return -EINVAL;

	/* get other socket */
	other = sk->protinfo.af_unix.other;

 	/* shutdown send */
	if (how & SEND_SHUTDOWN) {
		sk->protinfo.af_unix.shutdown |= SEND_SHUTDOWN;
		if (other)
			other->protinfo.af_unix.shutdown |= RCV_SHUTDOWN;
	}

	/* shutdown receive */
	if (how & RCV_SHUTDOWN) {
		sk->protinfo.af_unix.shutdown |= RCV_SHUTDOWN;
		if (other)
			other->protinfo.af_unix.shutdown |= SEND_SHUTDOWN;
	}

	return 0;
}

/*
 * Get peer name system call.
 */
static int unix_getpeername(struct socket_t *sock, struct sockaddr *addr, size_t *addrlen)
{
	unix_socket_t *sk, *other;

	/* get UNIX socket */
	sk = (unix_socket_t *) sock->data;
	if (!sk)
		return -EINVAL;

	/* copy destination address */
	other = sk->protinfo.af_unix.other;
	if (other) {
		memset(addr, 0, sizeof(struct sockaddr));
		memcpy(addr, &other->protinfo.af_unix.sunaddr, other->protinfo.af_unix.sunaddr_len);
		*addrlen = other->protinfo.af_unix.sunaddr_len;
	}

	return 0;
}

/*
 * Get sock name system call.
 */
static int unix_getsockname(struct socket_t *sock, struct sockaddr *addr, size_t *addrlen)
{
	unix_socket_t *sk;

	/* get UNIX socket */
	sk = (unix_socket_t *) sock->data;
	if (!sk)
		return -EINVAL;

	/* copy destination address */
	memset(addr, 0, sizeof(struct sockaddr));
	memcpy(addr, &sk->protinfo.af_unix.sunaddr, sk->protinfo.af_unix.sunaddr_len);
	*addrlen = sk->protinfo.af_unix.sunaddr_len;

	return 0;
}

/*
 * Get socket options system call.
 */
static int unix_getsockopt(struct socket_t *sock, int level, int optname, void *optval, size_t optlen)
{
	UNUSED(sock);
	UNUSED(level);
	UNUSED(optname);
	UNUSED(optval);
	UNUSED(optlen);

	printf("unix_getsockopt(%d) undefined\n", optname);

	return 0;
}

/*
 * Set socket options system call.
 */
static int unix_setsockopt(struct socket_t *sock, int level, int optname, void *optval, size_t optlen)
{
	UNUSED(sock);
	UNUSED(level);
	UNUSED(optname);
	UNUSED(optval);
	UNUSED(optlen);

	printf("unix_setsockopt(%d) undefined\n", optname);

	return 0;
}

/*
 * Connect a pair of sockets.
 */
static int unix_socketpair(struct socket_t *sock1, struct socket_t *sock2)
{
	unix_socket_t *sk1 = sock1->data, *sk2 = sock2->data;

	/* connect sockets */
	sk1->protinfo.af_unix.other = sk2;
	sk2->protinfo.af_unix.other = sk1;
	sock1->state = SS_CONNECTED;
	sock2->state = SS_CONNECTED;

	return 0;
}

/*
 * UNIX operations.
 */
struct prot_ops unix_ops = {
	.create		= unix_create,
	.dup		= unix_dup,
	.release	= unix_release,
	.poll		= unix_poll,
	.recvmsg	= unix_recvmsg,
	.sendmsg	= unix_sendmsg,
	.bind		= unix_bind,
	.accept		= unix_accept,
	.connect	= unix_connect,
	.shutdown	= unix_shutdown,
	.getpeername	= unix_getpeername,
	.getsockname	= unix_getsockname,
	.getsockopt	= unix_getsockopt,
	.setsockopt	= unix_setsockopt,
	.socketpair	= unix_socketpair,
};
