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
		task_sleep(&sock->wait);
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