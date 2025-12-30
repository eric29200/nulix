#include <net/sock.h>
#include <net/sk_buff.h>
#include <net/inet/tcp.h>
#include <proc/sched.h>
#include <stderr.h>
#include <stdio.h>

extern struct net_proto_family *net_families[NPROTO];

/*
 * Allocate a socket.
 */
struct sock *sk_alloc(int family, int zero_it)
{
	struct sock *sk;

	/* allocate socket */
	sk = (struct sock *) kmalloc(sizeof(struct sock));
	if (!sk)
		return NULL;

	/* zero it */
	if (zero_it)
		memset(sk, 0, sizeof(struct sock));

	/* set family */
	sk->family = family;

	return sk;
}

/*
 * Default socket wake up.
 */
void sock_def_wakeup(struct sock *sk)
{
	if (!sk->dead)
		wake_up(sk->sleep);
}

/*
 * Default socket wake up.
 */
void sock_def_readable(struct sock *sk, size_t len)
{
	UNUSED(len);

	if (!sk->dead)
		wake_up(sk->sleep);
}

/*
 * Init socket data.
 */
void sock_init_data(struct socket *sock, struct sock *sk)
{
	skb_queue_head_init(&sk->receive_queue);
	sk->rcvbuf = SK_RMEM_MAX;
	sk->sndbuf = SK_WMEM_MAX;
	sk->socket = sock;
	sk->state = TCP_CLOSE;

	if (sock) {
		sk->type = sock->type;
		sk->sleep = &sock->wait;
		sock->sk = sk;
	} else {
		sk->sleep = NULL;
	}

	/* default callbacks */
	sk->state_change = sock_def_wakeup;
	sk->data_ready = sock_def_readable;

	/* init creds */
	sock->sk->peercred.pid = 0;
	sock->sk->peercred.uid = -1;
	sock->sk->peercred.gid = -1;
}

/*
 * No dup.
 */
int sock_no_dup(struct socket *newsock, struct socket *oldsock)
{
	struct sock *sk = oldsock->sk;

	return net_families[sk->family]->create(newsock, sk->protocol);
}

/*
 * No listen.
 */
int sock_no_listen(struct socket *sock)
{
	UNUSED(sock);
	return -EOPNOTSUPP;
}

/*
 * No accept.
 */
int sock_no_accept(struct socket *sock, struct socket *sock_new, int flags)
{
	UNUSED(sock);
	UNUSED(sock_new);
	UNUSED(flags);
	return -EOPNOTSUPP;
}

/*
 * No getsockopt.
 */
int sock_no_getsockopt(struct socket *sock, int level, int optname, void *optval, size_t *optlen)
{
	UNUSED(sock);
	UNUSED(level);
	UNUSED(optname);
	UNUSED(optval);
	UNUSED(optlen);
	return -EOPNOTSUPP;
}

/*
 * No setsockopt.
 */
int sock_no_setsockopt(struct socket *sock, int level, int optname, void *optval, size_t optlen)
{
	UNUSED(sock);
	UNUSED(level);
	UNUSED(optname);
	UNUSED(optval);
	UNUSED(optlen);
	return -EOPNOTSUPP;
}

/*
 * Allocate a send buffer.
 */
struct sk_buff *sock_alloc_send_skb(struct sock *sk, size_t len, int nonblock, int *err)
{
	struct sk_buff *skb;

	/* reset error code */
	*err = 0;

	/* wait for space */
	for (;;) {
		/* socket shutdown */
		if (sk->shutdown & SEND_SHUTDOWN) {
			*err = -EPIPE;
			return NULL;
		}

		/* check space */
		if (sk->wmem_alloc < sk->sndbuf)
			break;

		/* non blocking */
		if (nonblock) {
			*err = -EAGAIN;
			return NULL;
		}

		/* signal interrupt */
		if (signal_pending(current_task)) {
			*err = -ERESTARTSYS;
			return NULL;
		}

		/* wait */
		sleep_on(sk->sleep);
	}

	/* allocate a socket buffer */
	skb = skb_alloc(len);
	if (!skb) {
		*err = -ENOMEM;
		return NULL;
	}

	/* set socket */
	skb->sk = sk;
	sk->wmem_alloc += len;

	return skb;
}
