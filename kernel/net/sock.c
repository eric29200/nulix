#include <net/sock.h>
#include <net/sk_buff.h>
#include <proc/sched.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Allocate a send buffer.
 */
struct sk_buff *sock_alloc_send_skb(struct socket *sock, size_t len, int nonblock, int *err)
{
	struct sk_buff *skb;

	/* reset error code */
	*err = 0;

	/* wait for space */
	for (;;) {
		/* socket shutdown */
		if (sock->sk->shutdown & SEND_SHUTDOWN) {
			*err = -EPIPE;
			return NULL;
		}

		/* check space */
		if (sock->sk->wmem_alloc < sock->sk->sndbuf)
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
		sleep_on(&sock->wait);
	}

	/* allocate a socket buffer */
	skb = skb_alloc(len);
	if (!skb) {
		*err = -ENOMEM;
		return NULL;
	}

	/* set socket */
	skb->sock = sock;
	skb->sock->sk->wmem_alloc += len;

	return skb;
}

/*
 * Duplicate a socket.
 */
int sock_no_dup(struct socket *sock, struct socket *sock_new)
{
	struct sock *sk = sock->sk;
	return net_families[sk->family]->create(sock_new, sk->protocol);
}

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