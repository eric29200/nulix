#include <net/inet/tcp.h>
#include <net/sock.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Receive a packet in non establised state.
 */
int tcp_rcv_state_process(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_header *th = skb->h.tcp_header;
	struct tcp_opt *tp = &sk->protinfo.af_tcp;

	switch (sk->state) {
		case TCP_SYN_SENT:
			if (th->ack) {
				/* bad ack */
				if (TCP_SKB_CB(skb)->ack_seq != tp->snd_nxt)
					return 1;

				/* we only want to receive a SYN message */
				if (!th->syn)
					return 0;

				/* set connection established */
				sk->state = TCP_ESTABLISHED;

				/* ack packet */
				tp->rcv_nxt = TCP_SKB_CB(skb)->seq + 1;
				tcp_send_ack(sk, 0, 0);

				/* wake up processes */
				wake_up(sk->sleep);
			}

			return 0;
		case TCP_FIN_WAIT1:
			/* we wait for an ack */
			if (!th->ack || TCP_SKB_CB(skb)->ack_seq != tp->snd_nxt)
				return 1;

			/* ack packet */
			tp->rcv_nxt = TCP_SKB_CB(skb)->seq + 1;
			tcp_send_ack(sk, 0, 0);

			/* update socket state */
			sk->shutdown |= SEND_SHUTDOWN;
			sk->state = TCP_FIN_WAIT2;

			/* set timer to close socket */
			tcp_set_timer(sk, TCP_TIME_CLOSE, TCP_FIN_TIMEOUT);
			break;
		default:
			printf("tcp_rcv_state_process: unknown socket state %d\n", sk->state);
			break;
	}

	return 0;
}

/*
 * Receive a packet in TCP_ESTABLISED state.
 */
int tcp_rcv_established(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_header *th = skb->h.tcp_header;
	struct tcp_opt *tp = &sk->protinfo.af_tcp;
	struct sk_buff *skb_new;
	size_t data_len;

	/* check if this is the packet we want */
	if (TCP_SKB_CB(skb)->seq != tp->rcv_nxt)
		return -EINVAL;

	/* SYN message in establised connection ? */
	if (th->syn)
		return -EINVAL;

	/* compute data length */
	data_len = tcp_data_length(skb);
	if (!data_len)
		return 0;

	/* send ACK message */
	tp->rcv_nxt = TCP_SKB_CB(skb)->end_seq;
	tcp_send_ack(sk, 0, 0);

	/* clone socket buffer */
	skb_new = skb_clone(skb);
	if (!skb_new)
		return -ENOMEM;

	/* add buffer to socket */
	skb_queue_tail(&sk->receive_queue, skb_new);

	/* wake up processes */
	wake_up(sk->sleep);

	return 0;
}