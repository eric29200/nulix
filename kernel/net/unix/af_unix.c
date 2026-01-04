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

#define MIN_WRITE_SPACE			2048
#define UNIX_DELETE_DELAY		(HZ)
#define UNIX_MAX_DGRAM_QLEN		10

#define unix_peer(sk)			((sk)->pair)
#define unix_our_peer(sk, osk)		(unix_peer(osk) == sk)
#define unix_may_send(sk, osk)		(unix_peer(osk) == NULL || unix_our_peer(sk, osk))

static struct sock *unix_create1(struct socket *sock, int protocol);
static void unix_destroy_socket(unix_socket_t *sk);

/* UNIX sockets */
static LIST_HEAD(unix_sockets);
static struct wait_queue *unix_dgram_wqueue = NULL;

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
 * Release a unix address.
 */
static void unix_release_addr(struct unix_address *addr)
{
	if (!addr)
		return;

	addr->refcnt--;
	if (!addr->refcnt)
		kfree(addr);
}

/*
 * Destroy unix address.
 */
static void unix_destruct_addr(struct sock *sk)
{
	unix_release_addr(sk->protinfo.af_unix.addr);
}

/*
 * check unix socket name.
 */
static int unix_mkname(struct sockaddr_un *sunaddr, size_t len)
{
	/* check size */
	if (len <= sizeof(short) || len > sizeof(struct sockaddr_un))
		return -EINVAL;

	/* check protocol */
	if (!sunaddr || sunaddr->sun_family != AF_UNIX)
		return -EINVAL;

	/* limit name to maximum size */
	if (sunaddr->sun_path[0]) {
		if (len > sizeof(*sunaddr))
			len = sizeof(*sunaddr);
		((char *) sunaddr)[len] = 0;
		len = strlen(sunaddr->sun_path) + 1 + sizeof(short);
		return len;
	}

	return len;
}

/*
 * Find a UNIX socket.
 */
static unix_socket_t *unix_find_socket_by_inode(struct inode *inode)
{
	struct list_head *pos;
	struct dentry *dentry;
	unix_socket_t *sk;

	list_for_each(pos, &unix_sockets) {
		sk = list_entry(pos, unix_socket_t, list);
		dentry = sk->protinfo.af_unix.dentry;

		if (dentry && dentry->d_inode == inode) {
			unix_lock(sk);
			return sk;
		}
	}

	return NULL;
}

/*
 * Find a UNIX socket.
 */
static unix_socket_t *unix_find_socket_by_name(struct sockaddr_un *sunaddr, size_t addrlen, int type)
{
	struct list_head *pos;
	unix_socket_t *sk;

	list_for_each(pos, &unix_sockets) {
		sk = list_entry(pos, unix_socket_t, list);

		if (sk->protinfo.af_unix.addr->len == addrlen && memcmp(&sk->protinfo.af_unix.addr->name, sunaddr, addrlen) == 0 && sk->type == type) {
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
		res = unix_find_socket_by_name(sunaddr, addrlen, type);
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

	/* wake up processes waiting for space */
	if (sk->type == SOCK_DGRAM)
		wake_up(&unix_dgram_wqueue);

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
		sk_free(sk);
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
		sk_free(sk);
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

	/* exceptional events ? */
	if (sk->err)
		mask |= POLLERR;
	if (sk->shutdown & RCV_SHUTDOWN)
		mask |= POLLHUP;

	/* readable ? */
	if (!skb_queue_empty(&sk->receive_queue))
		mask |= POLLIN | POLLRDNORM;

	/* connection-based need to check for termination and startup */
	if (sk->type == SOCK_STREAM && sk->state == TCP_CLOSE)
		mask |= POLLHUP;

	/* writable ? */
	if (sk->sndbuf - sk->wmem_alloc >= MIN_WRITE_SPACE)
		mask |= POLLOUT | POLLWRNORM | POLLWRBAND;

	/* add wait queue to select table */
	select_wait(&sock->wait, wait);

	return mask;
}

/*
 * Receive a message.
 */
static int unix_dgram_recvmsg(struct socket *sock, struct msghdr *msg, size_t size)
{
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

	/* wake up processes waiting for space */
	wake_up(&unix_dgram_wqueue);

	/* set address */
	if (msg->msg_name) {
		msg->msg_namelen = sizeof(short);

		if (skb->sk->protinfo.af_unix.addr) {
			msg->msg_namelen = skb->sk->protinfo.af_unix.addr->len;
			memcpy(msg->msg_name, skb->sk->protinfo.af_unix.addr->name, skb->sk->protinfo.af_unix.addr->len);
		}
	}

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
	int noblock = msg->msg_flags & MSG_DONTWAIT;
	struct sockaddr_un *sunaddr = msg->msg_name;
	size_t target = 1, chunk, copied = 0;
	struct sock *sk = sock->sk;
	struct sk_buff *skb;

	/* check flags */
	if (sock->flags & SO_ACCEPTCON)
		return -EINVAL;
	if (msg->msg_flags & MSG_OOB)
		return -EOPNOTSUPP;
	if (msg->msg_flags & MSG_WAITALL)
		target = size;

	/* reset address */
	msg->msg_namelen = 0;

	do {
		/* get next message or wait */
		skb = skb_dequeue(&sk->receive_queue);
		if (!skb) {
			/* done */
			if (copied >= target)
				break;

			/* socket error */
			if (sk->err)
				return sock_error(sk);

			/* socket is down */
			if (sk->shutdown & RCV_SHUTDOWN)
				break;

			/* non blocking */
			if (noblock)
				return -EAGAIN;

			/* wait for a message */
			sleep_on(&sk->socket->wait);

			/* handle signal */
			if (signal_pending(current_task))
				return -ERESTARTSYS;

			continue;
		}

		/* copy address just once */
		if (sunaddr) {
			msg->msg_namelen = sizeof(short);

			if (skb->sk->protinfo.af_unix.addr) {
				msg->msg_namelen=skb->sk->protinfo.af_unix.addr->len;
				memcpy(sunaddr, skb->sk->protinfo.af_unix.addr->name, skb->sk->protinfo.af_unix.addr->len);
			}

			sunaddr = NULL;
		}

		/* copy data */
		chunk = skb->len <= size ? skb->len : size;
		memcpy_toiovec(msg->msg_iov, skb->data, chunk);
		copied += chunk;
		size -= chunk;

		/* free message or requeue it */
		if (!(msg->msg_flags & MSG_PEEK)) {
			skb_pull(skb, chunk);

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
	} while (size);

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
	int ret, namelen;

	/* check flags */
	if (msg->msg_flags & MSG_OOB)
		return -EOPNOTSUPP;
	if (msg->msg_flags & ~(MSG_DONTWAIT | MSG_NOSIGNAL))
		return -EINVAL;

	/* get destination */
	if (msg->msg_namelen) {
		namelen = unix_mkname(sunaddr, msg->msg_namelen);
		if (namelen < 0)
			return namelen;
	} else {
		sunaddr = NULL;
		if (!unix_peer(sk))
			return -ENOTCONN;
	}

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
dead:
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

		other = unix_find_other(sunaddr, namelen, sk->type, &ret);
		if (!other)
			goto err;

		ret = -EINVAL;
		if (!unix_may_send(sk, other))
			goto err_unlock;
	}

	/* wait for space in other socket */
	while (skb_queue_len(&other->receive_queue) >= UNIX_MAX_DGRAM_QLEN) {
		/* non blocking */
		if (msg->msg_flags & MSG_DONTWAIT) {
			ret = -EAGAIN;
			goto err_unlock;
		}

		/* wait for space */
		sleep_on(&unix_dgram_wqueue);

		/* socket is dead */
		if (other->dead)
			goto dead;

		/* sock is down */
		if (sk->shutdown & SEND_SHUTDOWN) {
			ret = -EPIPE;
			goto err_unlock;
		}

		/* handle signal */
		if (signal_pending(current_task)) {
			ret = -ERESTARTSYS;
			goto err_unlock;
		}
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
	if (msg->msg_namelen) {
		if (sk->state == TCP_ESTABLISHED)
			return -EISCONN;
		else
			return -EOPNOTSUPP;
	} else if (!unix_peer(sk)) {
		return -ENOTCONN;
	}

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
 * Auto bind a socket.
 */
static int unix_autobind(struct socket *sock)
{
	static uint32_t ordernum = 1;
	struct sock *sk = sock->sk;
	struct unix_address *addr;
	unix_socket_t *osk;

	/* allocate unix address */
	addr = kmalloc(sizeof(struct unix_address) + sizeof(short) + 16);
	if (!addr)
		return -ENOMEM;

	/* already bound */
	if (sk->protinfo.af_unix.addr || sk->protinfo.af_unix.dentry) {
		kfree(addr);
		return -EINVAL;
	}

	/* init address */
	memset(addr, 0, sizeof(struct unix_address) + sizeof(short) + 16);
	addr->name->sun_family = AF_UNIX;
	addr->refcnt = 1;

	/* try to find a free name */
	for (;;) {
		addr->len = sprintf(addr->name->sun_path + 1, "%08x", ordernum) + 1 + sizeof(short);
		ordernum++;

		/* check if socket name already exist */
		osk = unix_find_socket_by_name(addr->name, addr->len, sock->type);
		if (!osk)
			break;

		/* retry */
		unix_unlock(osk);
	}

	/* set address */
	sk->protinfo.af_unix.addr = addr;

	return 0;
}

/*
 * Bind system call (attach an address to a socket).
 */
static int unix_bind(struct socket *sock, const struct sockaddr *uaddr, size_t addrlen)
{
	struct sockaddr_un *sunaddr = (struct sockaddr_un *) uaddr;
	struct sock *sk = sock->sk;
	struct unix_address *addr;
	struct dentry *dentry;
	unix_socket_t *osk;
	int alen, ret;

	/* already bound */
	if (sk->protinfo.af_unix.addr || sk->protinfo.af_unix.dentry || sunaddr->sun_family != AF_UNIX)
		return -EINVAL;

	/* no name : auto bind */
	if (addrlen == sizeof(short))
		return unix_autobind(sock);

	/* make unix name */
	alen = unix_mkname(sunaddr, addrlen);
	if (alen < 0)
		return alen;

	/* allocate unix address */
	addr = kmalloc(sizeof(struct unix_address) + alen);
	if (!addr)
		return -ENOMEM;

	memcpy(addr->name, sunaddr, alen);
	addr->len = alen;
	addr->refcnt = 1;

	/* abstract socket */
	if (!sunaddr->sun_path[0]) {
		osk = unix_find_socket_by_name(sunaddr, alen, sk->type);
		if (osk) {
			unix_unlock(osk);
			kfree(addr);
			return -EADDRINUSE;
		}

		sk->protinfo.af_unix.addr = addr;
		return 0;
	}

	/* make socket node */
	sk->protinfo.af_unix.addr = addr;
	dentry = do_mknod(AT_FDCWD, sunaddr->sun_path, S_IFSOCK | sock->inode->i_mode, 0);
	if (IS_ERR(dentry)) {
		ret = PTR_ERR(dentry);
		unix_release_addr(addr);
		sk->protinfo.af_unix.addr = NULL;
		return ret == -EEXIST ? -EADDRINUSE : ret;
	}

	/* set dentry */
	sk->protinfo.af_unix.dentry = dentry;

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
	if (!sk->protinfo.af_unix.addr)
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
	int ret, alen;

	/* get UNIX socket */
	sk = sock->sk;
	if (!sk)
		return -EINVAL;

	/* make destination address */
	alen = unix_mkname(sunaddr, addrlen);
	if (alen < 0)
		return alen;

	/* find other unix socket */
	other = unix_find_other(sunaddr, alen, sk->type, &ret);
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
	int ret, alen;

	/* make destination address */
	alen = unix_mkname(sunaddr, addrlen);
	if (alen < 0)
		return alen;

	/* find listening sock */
	other = unix_find_other(sunaddr, alen, sk->type, &ret);
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
	if (!sk->protinfo.af_unix.addr)
		unix_autobind(sock);
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
	if (other->protinfo.af_unix.addr) {
		other->protinfo.af_unix.addr->refcnt += 1;
		sk_new->protinfo.af_unix.addr = other->protinfo.af_unix.addr;
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
	struct sockaddr_un *sunaddr = (struct sockaddr_un *) addr;
	struct sock *sk = sock->sk;

	/* get peer name ? */
	if (peer) {
		if (!unix_peer(sk))
			return -ENOTCONN;
		sk = unix_peer(sk);
	}

	/* not bound */
	if (!sk->protinfo.af_unix.addr) {
		sunaddr->sun_family = AF_UNIX;
		sunaddr->sun_path[0] = 0;
		*addrlen = sizeof(short);
		return 0;
	}

	/* get name */
	*addrlen = sk->protinfo.af_unix.addr->len;
	memcpy(sunaddr, sk->protinfo.af_unix.addr->name, *addrlen);

	return 0;
}

/*
 * Ioctl on a UNIX socket.
 */
static int unix_ioctl(struct socket *sock, int cmd, unsigned long arg)
{
	UNUSED(sock);
	UNUSED(cmd);
	UNUSED(arg);
	return -ENOIOCTLCMD;
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
	sk->destruct = unix_destruct_addr;
	sk->protinfo.af_unix.dentry=NULL;

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
