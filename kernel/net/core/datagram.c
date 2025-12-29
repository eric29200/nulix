#include <net/sock.h>
#include <proc/sched.h>
#include <stderr.h>

/*
 * Receive a datagram packet.
 */
struct sk_buff *skb_recv_datagram(struct sock *sk, int flags, int noblock, int *err)
{
	struct sk_buff *skb;
	int ret = 0;

	/* wait for a packet */
	while (skb_queue_empty(&sk->receive_queue)) {
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
		sleep_on(&sk->sock->wait);
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