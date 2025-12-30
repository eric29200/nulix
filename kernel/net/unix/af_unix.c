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

#define UNIX_DELETE_DELAY		(HZ)

#define unix_peer(sk)			((sk)->pair)
#define unix_our_peer(sk, osk)		(unix_peer(osk) == sk)
#define unix_may_send(sk, osk)		(unix_peer(osk) == NULL || unix_our_peer(sk, osk))

static struct sock *unix_create1(struct socket *sock, int protocol);
static void unix_destroy_socket(unix_socket_t *sk);

/* UNIX sockets */
static LIST_HEAD(unix_sockets);

/*
 * Lock a unix socket.
 */
static void unix_lock(unix_socket_t *sk)
{
	sk->sock_readers++;
}

/*
 * Unlock a unix socket.
 */
static void unix_unlock(unix_socket_t *sk)
{
	sk->sock_readers--;
}

/*
 * Is a unix socket locked ?
 */
static int unix_locked(unix_socket_t *sk)
{
	return sk->sock_readers;
}

/*
 * Find a UNIX socket.
 */
static unix_socket_t *unix_find_socket_by_inode(struct inode *inode)
{
	struct list_head *pos;
	unix_socket_t *sk;

	list_for_each(pos, &unix_sockets) {
		sk = list_entry(pos, unix_socket_t, list);
		if (sk && sk->protinfo.af_unix.dentry && sk->protinfo.af_unix.dentry->d_inode == inode) {
			unix_lock(sk);
			return sk;
		}
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
		if (sk && sk->protinfo.af_unix.sunaddr_len == addrlen && memcmp(&sk->protinfo.af_unix.sunaddr, sunaddr, addrlen) == 0) {
			unix_lock(sk);
			return sk;
		}
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

		/* release dentry */
		dput(dentry);

		/* handle error */
		if (res && res->type != type) {
			*err = -EPROTOTYPE;
			unix_unlock(res);
			return NULL;
		}
	}

	/* handle error */
	if (!res)
		*err = -ECONNREFUSED;

	return res;
}

/*
 * Release a socket.
 */
static int unix_release_sock(unix_socket_t *sk)
{
	unix_socket_t *pair;

	/* update socket */
	sk->state_change(sk);
	sk->dead = 1;
	sk->socket = NULL;

	/* shutdown pair socket */
	pair = unix_peer(sk);
	if (pair) {
		if (sk->type == SOCK_STREAM && unix_our_peer(sk, pair)) {
			pair->data_ready(pair, 0);
			pair->shutdown = SHUTDOWN_MASK;
		}

		unix_unlock(pair);
	}

	/* destroy socket */
	unix_destroy_socket(sk);

	return 0;
}

/*
 * Delayed destruction callback.
 */
static void unix_destroy_timer(void *arg)
{
	unix_socket_t *sk = (unix_socket_t *) arg;

	/* try to destroy socket */
	if (!unix_locked(sk) && sk->wmem_alloc == 0) {
		kfree(sk);
		return;
	}

	/* reschedule timer */
	init_timer(&sk->timer, unix_destroy_timer, sk, jiffies + UNIX_DELETE_DELAY);
	add_timer(&sk->timer);
}

/*
 * Delay socket destruction.
 */
static void unix_delayed_delete(unix_socket_t *sk)
{
	init_timer(&sk->timer, unix_destroy_timer, sk, jiffies + UNIX_DELETE_DELAY);
	add_timer(&sk->timer);
}

/*
 * Destroy a socket.
 */
static void unix_destroy_socket(unix_socket_t *sk)
{
	struct sk_buff *skb;

	/* remove socket */
	list_del(&sk->list);

	/* free remaining packets in receive queue */
	while ((skb = skb_dequeue(&sk->receive_queue)) != NULL) {
		if (sk->state == TCP_LISTEN)
			unix_release_sock(skb->sk);
		skb_free(skb);
	}

	/* release inode */
	if (sk->protinfo.af_unix.dentry) {
		dput(sk->protinfo.af_unix.dentry);
		sk->protinfo.af_unix.dentry = NULL;
	}

	/* free unix socket (or delay it) */
	if (!unix_locked(sk) && sk->wmem_alloc == 0) {
		kfree(sk);
	} else {
		sk->state = TCP_CLOSE;
		sk->dead = 1;
		unix_delayed_delete(sk);
	}
}

/*
 * Release a socket.
 */
static int unix_release(struct socket *sock)
{
	unix_socket_t *sk;

	/* get UNIX socket */
	sk = sock->sk;
	if (!sk)
		return 0;

	/* update state */
	sock->sk = NULL;
	if (sock->state != SS_UNCONNECTED)
		sock->state = SS_DISCONNECTING;

	/* release socket */
	return unix_release_sock(sk);
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
 * Send a datagram message.
 */
static int unix_dgram_sendmsg(struct socket *sock, const struct msghdr *msg, size_t len)
{
	struct sockaddr_un *sunaddr = msg->msg_name;
	struct sock *sk = sock->sk;
	unix_socket_t *other;
	struct sk_buff *skb;
	int ret;

	/* check flags */
	if (msg->msg_flags & MSG_OOB)
		return -EOPNOTSUPP;
	if (msg->msg_flags & ~(MSG_DONTWAIT | MSG_NOSIGNAL))
		return -EINVAL;

	/* check connection */
	if (!msg->msg_namelen && !unix_peer(sk))
		return -ENOTCONN;

	/* check message size */
	if (len > sk->sndbuf)
		return -EMSGSIZE;

	/* allocate a socket buffer */
	skb = sock_alloc_send_skb(sk, len, msg->msg_flags & MSG_DONTWAIT, &ret);
	if (!skb)
		return ret;

	/* copy data to socket buffer */
	UNIXCB(skb).attr = msg->msg_flags;
	skb->h.raw = skb->data;
	memcpy_fromiovec(skb_put(skb, len), msg->msg_iov, len);

	/* get peer socket */
	other = unix_peer(sk);
	if (other && other->dead) {
		unix_unlock(other);
		unix_peer(sk) = NULL;
		other = NULL;
		ret = -ECONNRESET;
		if (!sunaddr)
			goto err;
	}

	/* if not connected, find other socket */
	if (!other) {
		ret = -ECONNRESET;
		if (!sunaddr)
			goto err;

		other = unix_find_other(sunaddr, msg->msg_namelen, sk->type, &ret);
		if (!other)
			goto err;

		ret = -EINVAL;
		if (!unix_may_send(sk, other))
			goto err_unlock;
	}

	/* queue message in other socket */
	skb_queue_tail(&other->receive_queue, skb);
	other->data_ready(other, len);

	/* not connected : unlock other socket */
	if (!unix_peer(sk))
		unix_unlock(other);

	return len;
err_unlock:
	unix_unlock(other);
err:
	skb_free(skb);
	return ret;
}

/*
 * Send a stream message.
 */
static int unix_stream_sendmsg(struct socket *sock, const struct msghdr *msg, size_t len)
{
	struct sock *sk = sock->sk;
	size_t sent = 0, size;
	unix_socket_t *other;
	struct sk_buff *skb;
	int ret;

	/* check connection and flags */
	if (sock->flags & SO_ACCEPTCON)
		return -EINVAL;
	if (msg->msg_flags & MSG_OOB)
		return -EOPNOTSUPP;
	if (msg->msg_flags & ~(MSG_DONTWAIT | MSG_NOSIGNAL))
		return -EINVAL;

	/* check connection */
	if (msg->msg_namelen)
		return -EOPNOTSUPP;
	if (!unix_peer(sk))
		return -ENOTCONN;

	/* socket is down */
	if (sk->shutdown & SEND_SHUTDOWN) {
		if (!(msg->msg_flags & MSG_NOSIGNAL))
			send_sig(current_task, SIGPIPE, 0);
		return -EPIPE;
	}

	while (sent < len) {
		/* keep two messages in the pipe so it schedules better */
		size = len - sent;
		if (size > sk->sndbuf / 2 - 16)
			size = sk->sndbuf / 2 - 16;

		/* allocate a socket buffer */
		skb = sock_alloc_send_skb(sk, size, msg->msg_flags & MSG_DONTWAIT, &ret);
		if (!skb)
			return sent ? (int) sent : ret;

		/* copy data to socket buffer */
		if (skb_tailroom(skb) < (int) size)
			size = skb_tailroom(skb);
		UNIXCB(skb).attr = msg->msg_flags;
		memcpy_fromiovec(skb_put(skb, size), msg->msg_iov, size);

		/* chec if peer socket is down */
		other = unix_peer(sk);
		if (other->dead || (sk->shutdown & SEND_SHUTDOWN)) {
			skb_free(skb);
			if (sent)
				return sent;
			if (!(msg->msg_flags & MSG_NOSIGNAL))
				send_sig(current_task, SIGPIPE, 0);
			return -EPIPE;
		}

		/* queue message in other socket */
		skb_queue_tail(&other->receive_queue, skb);
		other->data_ready(other, size);
		sent += size;
	}

	return sent;
}

/*
 * Bind system call (attach an address to a socket).
 */
static int unix_bind(struct socket *sock, const struct sockaddr *addr, size_t addrlen)
{
	struct sockaddr_un *sunaddr = (struct sockaddr_un *) addr;
	unix_socket_t *sk, *osk;
	struct dentry *dentry;

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
		osk = unix_find_socket_by_name(sunaddr, addrlen);
		if (osk) {
			unix_unlock(osk);
			return -EADDRINUSE;
		}
	} else {
		/* create socket file */
		dentry = do_mknod(AT_FDCWD, sunaddr->sun_path, S_IFSOCK | S_IRWXUGO, 0);
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
	unix_socket_t *sk = sock->sk, *sk_new = sock_new->sk;
	struct sk_buff *skb;
	unix_socket_t *tsk;

	/* check socket state */
	if (sock->state != SS_UNCONNECTED)
		return -EINVAL;
	if (!(sock->flags & SO_ACCEPTCON))
		return -EINVAL;
	if (sock->type != SOCK_STREAM)
		return -EOPNOTSUPP;
	if (sk->state != TCP_LISTEN)
		return -EINVAL;

	for (;;) {
		/* get next message */
		skb = skb_dequeue(&sk->receive_queue);
		if (!skb) {
			/* non blocking */
			if (flags & O_NONBLOCK)
				return -EAGAIN;

			/* wait for a message */
			sleep_on(sk->sleep);

			/* handle pending signal */
			if (signal_pending(current_task))
				return -ERESTARTSYS;

			continue;
		}

		/* ignore non SYN messages */
		if (!(UNIXCB(skb).attr & MSG_SYN)) {
			tsk = skb->sk;
			tsk->state_change(tsk);
			skb_free(skb);
			continue;
		}

		/* found SYN message */
		tsk = skb->sk;
		skb_free(skb);
		break;
	}


	/* attach accepted sock to socket */
	sock_new->state = SS_CONNECTED;
	sock_new->sk = tsk;
	tsk->sleep = sk_new->sleep;
	tsk->socket = sock_new;

	/* destroy handed sock */
	sk_new->socket = NULL;
	unix_destroy_socket(sk_new);

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

	/* check permissions */
	if (!unix_may_send(sk, other)) {
		unix_unlock(other);
		return -EINVAL;
	}

	/* if it was connected, reconnect */
	if (unix_peer(sk)) {
		unix_unlock(unix_peer(sk));
		unix_peer(sk) = NULL;
	}

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
	struct sock *sk = sock->sk, *sk_new;
	struct sk_buff *skb = NULL;
	unix_socket_t *other;
	int ret;

	/* find listening sock */
	other = unix_find_other(sunaddr, addrlen, sk->type, &ret);
	if (!other)
		return -ECONNREFUSED;

	/* create new sock for complete connection */
	sk_new = unix_create1(NULL, 1);

	/* allocate a socket buffer for sending to listening sock */
	if (sk_new) {
		skb = skb_alloc(0);
		if (skb) {
			skb->sk = sk_new;
			UNIXCB(skb).attr = MSG_SYN;
		}
	}

	/* check socket state */
	if (sock->state != SS_UNCONNECTED) {
		ret = sock->state == SS_CONNECTED ? -EISCONN : -EINVAL;
		goto out;
	}

	/* check socket state */
	ret = -EINVAL;
	if (sk->state != TCP_CLOSE)
		goto out;

	/* check if listener is in valid state */
	ret = -ECONNREFUSED;
	if (other->dead || other->state != TCP_LISTEN)
		goto out;

	/* no way to create a socket buffer */
	ret = -ENOMEM;
	if (!sk_new || !skb)
		goto out;

	/* set up connecting socket */
	sock->state = SS_CONNECTED;
	unix_peer(sk) = sk_new;
	unix_lock(sk);
	sk->state = TCP_ESTABLISHED;
	sk->peercred = other->peercred;

	/* set up newly created sock */
	unix_peer(sk_new) = sk;
	unix_lock(sk_new);
	sk_new->state = TCP_ESTABLISHED;
	sk_new->type = SOCK_STREAM;
	sk_new->peercred.pid = current_task->pid;
	sk_new->peercred.uid = current_task->euid;
	sk_new->peercred.gid = current_task->egid;

	/* copy address information from listening to new sock*/
	if (other->protinfo.af_unix.sunaddr_len) {
		sk_new->protinfo.af_unix.sunaddr_len = other->protinfo.af_unix.sunaddr_len;
		memcpy(&sk_new->protinfo.af_unix.sunaddr, &other->protinfo.af_unix.sunaddr, other->protinfo.af_unix.sunaddr_len);
	}

	/* update dentry reference count */
	if (other->protinfo.af_unix.dentry)
		sk_new->protinfo.af_unix.dentry = dget(other->protinfo.af_unix.dentry);

	/* send info to listening sock */
	skb_queue_tail(&other->receive_queue,skb);
	other->data_ready(other, 0);
	unix_unlock(other);
	return 0;
out:
	if (skb)
		skb_free(skb);
	if (sk_new)
		unix_destroy_socket(sk_new);
	if (other)
		unix_unlock(other);
	return ret;
}

/*
 * Shutdown system call.
 */
static int unix_shutdown(struct socket *sock, int mode)
{
	struct sock *sk = sock->sk;
	unix_socket_t *other;
	int peer_mode = 0;

	/* fix mode */
	mode = (mode + 1) & (RCV_SHUTDOWN | SEND_SHUTDOWN);
	if (mode)
		return 0;

	/* shutdown socket */
	sk->shutdown |= mode;
	sk->state_change(sk);

	/* shutdown peer socket */
	other = unix_peer(sk);
	if (other && sk->type == SOCK_STREAM && unix_our_peer(sk, other)) {
		if (mode & RCV_SHUTDOWN)
			peer_mode |= SEND_SHUTDOWN;
		if (mode & SEND_SHUTDOWN)
			peer_mode |= RCV_SHUTDOWN;
		other->shutdown |= peer_mode;

		if (peer_mode & RCV_SHUTDOWN)
			other->data_ready(other, 0);
		else
			other->state_change(other);
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
	.sendmsg	= unix_dgram_sendmsg,
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
	.sendmsg	= unix_stream_sendmsg,
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
