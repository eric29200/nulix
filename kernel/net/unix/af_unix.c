#include <net/unix/af_unix.h>
#include <net/inet/tcp.h>
#include <net/sk_buff.h>
#include <mm/mm.h>
#include <fs/fs.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>
#include <fcntl.h>
#include <uio.h>

#define unix_peer(sk)		((sk)->pair)


/* UNIX sockets */
static LIST_HEAD(unix_sockets);

/*
 * Find a UNIX socket.
 */
static unix_socket_t *unix_find_socket_by_inode(struct inode *inode)
{
	struct list_head *pos;
	unix_socket_t *sk;

	list_for_each(pos, &unix_sockets) {
		sk = list_entry(pos, unix_socket_t, list);
		if (sk && sk->protinfo.af_unix.dentry && sk->protinfo.af_unix.dentry->d_inode == inode)
			return sk;
	}

	return NULL;
}

/*
 * Find a UNIX socket.
 */
static unix_socket_t *unix_find_socket_by_name(struct sockaddr_un *sunaddr, size_t addrlen)
{
	struct list_head *pos;
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
static unix_socket_t *unix_find_other(struct sockaddr_un *sunaddr, size_t addrlen, int type, int *err)
{
	struct dentry *dentry;
	unix_socket_t *res;

	/* abstract socket */
	if (!sunaddr->sun_path[0]) {
		res = unix_find_socket_by_name(sunaddr, addrlen);
		if (!res) {
			dentry = res->protinfo.af_unix.dentry;
			if (dentry)
				update_atime(dentry->d_inode);
		}
	} else {
		/* resolve socket path */
		dentry = open_namei(AT_FDCWD, sunaddr->sun_path, S_IFSOCK, 0);
		if (IS_ERR(dentry)) {
			*err = PTR_ERR(dentry);
			return NULL;
		}

		/* find UNIX socket */
		res = unix_find_socket_by_inode(dentry->d_inode);
		if (res && res->type == type)
			update_atime(dentry->d_inode);
		else if (res && res->type != type) {
			*err = -EPROTOTYPE;
			return NULL;
		}

		/* release dentry */
		dput(dentry);
	}

	/* handle error */
	if (!res)
		*err = -ECONNREFUSED;

	return res;
}

/*
 * Release a socket.
 */
static int unix_release(struct socket *sock)
{
	unix_socket_t *sk, *other;

	/* get UNIX socket */
	sk = sock->sk;
	if (!sk)
		return 0;

	/* shutdown other */
	other = unix_peer(sk);
	if (other && sock->type == SOCK_STREAM)
		other->shutdown |= (RCV_SHUTDOWN | SEND_SHUTDOWN);

	/* release inode */
	if (sk->protinfo.af_unix.dentry) {
		dput(sk->protinfo.af_unix.dentry);
		sk->protinfo.af_unix.dentry = NULL;
	}

	/* release UNIX socket */
	list_del(&sk->list);
	kfree(sk);

	return 0;
}

/*
 * Poll on a socket.
 */
static int unix_poll(struct socket *sock, struct select_table *wait)
{
	unix_socket_t *sk;
	int mask = 0;

	/* get inet socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* check if there is a message in the queue */
	if (!skb_queue_empty(&sk->receive_queue) || sk->shutdown & RCV_SHUTDOWN)
		mask |= POLLIN;

	/* check if socket can write */
	if ((sk->socket->state != SS_DEAD && sk->wmem_alloc < sk->sndbuf) || sk->shutdown & SEND_SHUTDOWN)
		mask |= POLLOUT;

	/* add wait queue to select table */
	select_wait(&sock->wait, wait);

	return mask;
}

/*
 * Receive a message.
 */
static int unix_dgram_recvmsg(struct socket *sock, struct msghdr *msg, size_t size)
{
	struct sockaddr_un *sunaddr = msg->msg_name;
	struct sk_buff *skb;
	unix_socket_t *sk;
	int err;

	/* get UNIX socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* receive a packet */
	skb = skb_recv_datagram(sk, msg->msg_flags, msg->msg_flags & MSG_DONTWAIT, &err);
	if (!skb)
		return err;

	/* set address */
	if (sunaddr && skb->sk)
		memcpy(sunaddr, &skb->sk->protinfo.af_unix.sunaddr, skb->sk->protinfo.af_unix.sunaddr_len);

	/* check size */
	if (size > skb->len)
		size = skb->len;
	else if (size < skb->len)
		msg->msg_flags |= MSG_TRUNC;

	/* copy data */
	skb_copy_datagram_iovec(skb, 0, msg->msg_iov, size);

	/* free packet */
	skb_free(skb);

	return size;
}

/*
 * Receive a stream message.
 */
static int unix_stream_recvmsg(struct socket *sock, struct msghdr *msg, size_t size)
{
	size_t ct = msg->msg_iovlen, copied = 0, done, len, num;
	struct sockaddr_un *sunaddr = msg->msg_name;
	struct iovec *iov = msg->msg_iov;
	unix_socket_t *sk, *from;
	struct sk_buff *skb;
	uint8_t *buf;

	/* get UNIX socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* for each iov */
	while (ct--) {
		done = 0;
		buf = iov->iov_base;
		len = iov->iov_len;
		iov++;

		/* try to fill buffer */
		while (done < len) {
			/* done */
			if (copied == size || (copied && (msg->msg_flags & MSG_PEEK)))
				goto out;

			/* wait for a message */
			skb = skb_dequeue(&sk->receive_queue);
			if (!skb) {
				/* socket is down */
				if (sk->shutdown & RCV_SHUTDOWN)
					goto out;

				/* return partial data */
				if (copied)
					goto out;

				/* non blocking */
				if (msg->msg_flags & MSG_DONTWAIT)
					return -EAGAIN;

				/* signal received : restart system call */
				if (signal_pending(current_task))
					return -ERESTARTSYS;

				/* wait for a message */
				sleep_on(&sk->socket->wait);
				continue;
			}

			/* set address just once */
			if (sunaddr && skb->sk) {
				from = skb->sk;
				memcpy(sunaddr, &from->protinfo.af_unix.sunaddr, from->protinfo.af_unix.sunaddr_len);
				sunaddr = NULL;
			}

			/* copy message */
			num = skb->len <= len - done ? skb->len : len - done;
			memcpy(buf, skb->data, num);

			/* update sizes */
			copied += num;
			done += num;
			buf += num;

			/* free message or requeue it */
			if (!(msg->msg_flags & MSG_PEEK)) {
				skb_pull(skb, num);

				/* partial read */
				if (skb->len > 0) {
					skb_queue_head(&sk->receive_queue, skb);
					break;
				}

				/* free socket buffer */
				skb_free(skb);
			} else {
				skb_queue_head(&sk->receive_queue, skb);
			}
		}
	}

out:
	return copied;
}

/*
 * Send a message.
 */
static int unix_sendmsg(struct socket *sock, const struct msghdr *msg, size_t size)
{
	struct sockaddr_un *sunaddr = msg->msg_name;
	unix_socket_t *sk, *other;
	struct sk_buff *skb;
	size_t sent, len;
	int ret;

	/* get UNIX socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* find other socket */
	if (sunaddr) {
		other = unix_find_other(sunaddr, msg->msg_namelen, sk->type, &ret);
		if (!other)
			return ret;
	} else {
		other = unix_peer(sk);
		if (!other)
			return -ENOTCONN;
	}

	for (sent = 0; sent < size;) {
		/* compute length */
		len = size - sent;
		if (len > sk->sndbuf)
			len = sk->sndbuf;

		/* allocate a socket buffer */
		skb = sock_alloc_send_skb(sk, len, msg->msg_flags & MSG_DONTWAIT, &ret);
		if (!skb)
			return sent ? (int) sent : ret;

		/* copy message */
		memcpy_fromiovec(skb_put(skb, len), msg->msg_iov, len);

		/* queue message to other socket */
		skb_queue_tail(&other->receive_queue, skb);

		/* wake up eventual readers */
		wake_up(&other->socket->wait);

		/* update sent */
		sent += len;
	}

	return sent;
}

/*
 * Bind system call (attach an address to a socket).
 */
static int unix_bind(struct socket *sock, const struct sockaddr *addr, size_t addrlen)
{
	struct sockaddr_un *sunaddr = (struct sockaddr_un *) addr;
	struct dentry *dentry;
	unix_socket_t *sk;
	int ret;

	/* check address length */
	if (addrlen < UN_PATH_OFFSET || addrlen > sizeof(struct sockaddr_un))
		return -EINVAL;

	/* get UNIX socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* socket already bound */
	if (sk->protinfo.af_unix.sunaddr_len || sk->protinfo.af_unix.dentry)
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

		/* resolve path */
		dentry = open_namei(AT_FDCWD, sunaddr->sun_path, 0, S_IFSOCK);
		if (IS_ERR(dentry))
			return PTR_ERR(dentry);

		/* set dentry */
		sk->protinfo.af_unix.dentry = dentry;
	}

	/* set socket address */
	memcpy(&sk->protinfo.af_unix.sunaddr, addr, addrlen);
	sk->protinfo.af_unix.sunaddr.sun_path[addrlen - UN_PATH_OFFSET] = 0;
	sk->protinfo.af_unix.sunaddr_len = addrlen;

	return 0;
}

/*
 * Listen system call.
 */
static int unix_listen(struct socket *sock)
{
	struct sock *sk = sock->sk;

	/* check socket */
	if (sock->state != SS_UNCONNECTED)
		return -EINVAL;
	if (sock->type != SOCK_STREAM)
		return -EOPNOTSUPP;
	if (!sk->protinfo.af_unix.sunaddr_len)
		return -EINVAL;

	/* set */
	sk->state = TCP_LISTEN;
	sock->flags |= SO_ACCEPTCON;

	/* set creds */
	sk->peercred.pid = current_task->pid;
	sk->peercred.uid = current_task->euid;
	sk->peercred.gid = current_task->egid;

	return 0;
}

/*
 * Accept system call.
 */
static int unix_accept(struct socket *sock, struct socket *sock_new, int flags)
{
	unix_socket_t *sk = sock->sk, *sk_new = sock_new->sk, *other;
	struct sk_buff *skb;

	/* socket must be listening */
	if (sock->state != SS_UNCONNECTED)
		return -EINVAL;
	if (!(sock->flags & SO_ACCEPTCON))
		return -EINVAL;
	if (sock->type != SOCK_STREAM)
		return -EOPNOTSUPP;
	if (sk->state != TCP_LISTEN)
		return -EINVAL;

	for (;;) {
		/* wait for a message */
		skb = skb_dequeue(&sk->receive_queue);
		if (!skb) {
			/* non blocking */
			if (flags & O_NONBLOCK)
				return -EAGAIN;

			/* disconnected : break */
			if (sock->state == SS_DISCONNECTING)
				return 0;

			/* signal received : restart system call */
			if (signal_pending(current_task))
				return -ERESTARTSYS;

			/* sleep */
			sleep_on(&sock->wait);
			continue;
		}

		/* ignore non SYN messages */
		if (!(UNIXCB(skb).attr & MSG_SYN)) {
			if (skb->sk)
				wake_up(skb->sk->sleep);
			skb_free(skb);
			continue;
		}

		/* get destination and free socket buffer */
		other = skb->sk;
		skb_free(skb);
		break;
	}

	/* set new socket */
	sock_new->state = SS_CONNECTED;
	unix_peer(sk_new) = other;
	sk_new->peercred.uid = sk->peercred.uid;

	/* set other socket connected and wake up eventual clients connecting */
	other->socket->state = SS_CONNECTED;
	unix_peer(other) = sk_new;
	wake_up(&unix_peer(sk_new)->socket->wait);

	return 0;
}

/*
 * Datagram connect.
 */
static int unix_dgram_connect(struct socket *sock, const struct sockaddr *addr, size_t addrlen)
{
	struct sockaddr_un *sunaddr = (struct sockaddr_un *) addr;
	unix_socket_t *sk, *other;
	int ret;

	/* get UNIX socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* find other unix socket */
	other = unix_find_other(sunaddr, addrlen, sk->type, &ret);
	if (!other)
		return ret;

	/* connect */
	unix_peer(sk) = other;

	return 0;
}

/*
 * Stream connect.
 */
static int unix_stream_connect(struct socket *sock, const struct sockaddr *addr, size_t addrlen)
{
	struct sockaddr_un *sunaddr = (struct sockaddr_un *) addr;
	unix_socket_t *sk, *other;
	struct sk_buff *skb;
	int ret;

	/* get UNIX socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* find other unix socket */
	other = unix_find_other(sunaddr, addrlen, sk->type, &ret);
	if (!other)
		return ret;

	/* create a SYN message */
	skb = skb_alloc(0);
	if (!skb)
		return -ENOMEM;
	UNIXCB(skb).attr = MSG_SYN;

	/* queue empty packet */
	skb->sk = sk;
	skb_queue_tail(&other->receive_queue, skb);

	/* wake up eventual reader */
	wake_up(&other->socket->wait);

	/* set socket */
	unix_peer(sk) = other;
	sock->state = SS_CONNECTING;

	/* wait for an accept */
	while (sock->state == SS_CONNECTING) {
		/* signal received : restart system call */
		if (signal_pending(current_task))
			return -ERESTARTSYS;

		/* sleep */
		sleep_on(&sock->wait);
	}

	/* set credentials */
	sock->sk->peercred = other->peercred;

	return 0;
}

/*
 * Shutdown system call.
 */
static int unix_shutdown(struct socket *sock, int how)
{
	unix_socket_t *sk, *other;

	/* get UNIX socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* get other socket */
	other = unix_peer(sk);

 	/* shutdown send */
	if (how & SEND_SHUTDOWN) {
		sk->shutdown |= SEND_SHUTDOWN;
		if (other)
			other->shutdown |= RCV_SHUTDOWN;
	}

	/* shutdown receive */
	if (how & RCV_SHUTDOWN) {
		sk->shutdown |= RCV_SHUTDOWN;
		if (other)
			other->shutdown |= SEND_SHUTDOWN;
	}

	return 0;
}

/*
 * Get socket name.
 */
static int unix_getname(struct socket *sock, struct sockaddr *addr, size_t *addrlen, int peer)
{
	struct sock *sk = sock->sk;

	/* get peer name ? */
	if (peer) {
		if (!unix_peer(sk))
			return -ENOTCONN;
		sk = unix_peer(sk);
	}

	/* get name */
	memset(addr, 0, sizeof(struct sockaddr));
	memcpy(addr, &sk->protinfo.af_unix.sunaddr, sk->protinfo.af_unix.sunaddr_len);
	*addrlen = sk->protinfo.af_unix.sunaddr_len;

	return 0;
}

/*
 * Ioctl on a UNIX socket.
 */
static int unix_ioctl(struct socket *sock, int cmd, unsigned long arg)
{
	UNUSED(sock);
	UNUSED(arg);

	printf("UNIX socket : unknown ioctl cmd 0x%x\n", cmd);

	return -EINVAL;
}

/*
 * UNIX datagram operations.
 */
static struct proto_ops unix_dgram_ops = {
	.dup		= sock_no_dup,
	.release	= unix_release,
	.poll		= unix_poll,
	.ioctl		= unix_ioctl,
	.recvmsg	= unix_dgram_recvmsg,
	.sendmsg	= unix_sendmsg,
	.bind		= unix_bind,
	.listen		= sock_no_listen,
	.accept		= sock_no_accept,
	.connect	= unix_dgram_connect,
	.shutdown	= unix_shutdown,
	.getname	= unix_getname,
	.getsockopt	= sock_no_getsockopt,
	.setsockopt	= sock_no_setsockopt,
};

/*
 * UNIX stream operations.
 */
static struct proto_ops unix_stream_ops = {
	.dup		= sock_no_dup,
	.release	= unix_release,
	.poll		= unix_poll,
	.ioctl		= unix_ioctl,
	.recvmsg	= unix_stream_recvmsg,
	.sendmsg	= unix_sendmsg,
	.bind		= unix_bind,
	.listen		= unix_listen,
	.accept		= unix_accept,
	.connect	= unix_stream_connect,
	.shutdown	= unix_shutdown,
	.getname	= unix_getname,
	.getsockopt	= sock_no_getsockopt,
	.setsockopt	= sock_no_setsockopt,
};

/*
 * Create a socket.
 */
static struct sock *unix_create1(struct socket *sock, int protocol)
{
	struct sock *sk;

	/* allocate UNIX socket */
	sk = sk_alloc(PF_UNIX, 1);
	if (!sk)
		return NULL;

	/* set socket */
	sk->protocol = protocol;
	sock_init_data(sock, sk);

	/* insert in sockets list */
	list_add_tail(&sk->list, &unix_sockets);

	return sk;
}

/*
 * Create a socket.
 */
static int unix_create(struct socket *sock, int protocol)
{
	/* check protocol */
	if (protocol && protocol != PF_UNIX)
		return -EPROTONOSUPPORT;

	/* set operations */
	switch (sock->type) {
		case SOCK_STREAM:
			sock->ops = &unix_stream_ops;
			break;
		case SOCK_RAW:
			sock->type = SOCK_DGRAM;
			sock->ops = &unix_dgram_ops;
			break;
		case SOCK_DGRAM:
			sock->ops = &unix_dgram_ops;
			break;
		default:
			return -ESOCKTNOSUPPORT;
	}

	return unix_create1(sock, protocol) ? 0 : -ENOMEM;
}

/*
 * UNIX protocol.
 */
static struct net_proto_family unix_family_ops = {
	.family		= PF_UNIX,
	.create		= unix_create,
};

/*
 * Init UNIX protocol.
 */
void unix_proto_init()
{
	sock_register(&unix_family_ops);
}
