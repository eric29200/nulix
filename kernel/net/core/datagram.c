#include <net/sock.h>
#include <proc/sched.h>
#include <net/inet/tcp.h>
#include <stderr.h>

/*
 * Receive a datagram packet.
 */
struct sk_buff *skb_recv_datagram(struct sock *sk, int flags, int noblock, int *err)
{
	struct sk_buff *skb;
	int ret = 0;

	/* socket error */
	ret = sock_error(sk);
	if (ret)
		goto no_packet;

	/* wait for a packet */
	while (skb_queue_empty(&sk->receive_queue)) {
		/* socket error */
		ret = sock_error(sk);
		if (ret)
			goto no_packet;

		/* socket is down */
		if (sk->shutdown & RCV_SHUTDOWN)
			goto no_packet;

		/* signal received : restart system call */
		ret = -ERESTARTSYS;
		if (signal_pending(current_task))
			goto no_packet;

		/* non blocking */
		ret = -EAGAIN;
		if (noblock)
			goto no_packet;

		/* wait for a message */
		sleep_on(sk->sleep);
	}

	/* peek or dequeue packet */
	if (flags & MSG_PEEK)
		skb = skb_peek(&sk->receive_queue);
	else
		skb = skb_dequeue(&sk->receive_queue);

	return skb;
no_packet:
	*err = ret;
	return NULL;
}

/*
 * Copy socket buffer's data to an iovec.
 */
void skb_copy_datagram_iovec(struct sk_buff *skb, int offset, struct iovec *to, size_t size)
{
	memcpy_toiovec(to, skb->h.raw + offset, size);
}

/*
 * Generic datagram poll function.
 */
int datagram_poll(struct sock *sk, struct select_table *wait)
{
	int mask = 0;

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
	select_wait(sk->sleep, wait);

	return mask;
}