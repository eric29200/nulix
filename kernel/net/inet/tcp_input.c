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

	switch (sk->state) {
		case TCP_SYN_SENT:
			if (th->ack) {
				if (!th->syn)
					return 0;

				/* set connection established */
				sk->state = TCP_ESTABLISHED;

				/* ack packet */
				sk->protinfo.af_tcp.ack_no = ntohl(skb->h.tcp_header->seq) + 1;
				tcp_send_ack(sk, 1, 0);

				/* wake up processes */
				wake_up(sk->sleep);
			}

			return 0;
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
	int ack_syn = 0, ack_fin = 0;
	struct sk_buff *skb_new;
	size_t data_len;

	/* compute data length */
	data_len = tcp_data_length(skb);

	/* set ack flags */
	if (skb->h.tcp_header->syn && !skb->h.tcp_header->ack)
		ack_syn = 1;
	else if (skb->h.tcp_header->fin && !skb->h.tcp_header->ack)
		ack_fin = 1;

	/* send ACK message */
	if (data_len > 0 || skb->h.tcp_header->syn || skb->h.tcp_header->fin) {
		sk->protinfo.af_tcp.ack_no = ntohl(skb->h.tcp_header->seq) + (data_len ? data_len : 1);
		tcp_send_ack(sk, ack_syn, ack_fin);
	}

	/* add message */
	if (data_len > 0 || th->fin) {
		/* clone socket buffer */
		skb_new = skb_clone(skb);
		if (!skb_new)
			return -ENOMEM;

		/* add buffer to socket */
		skb_queue_tail(&sk->receive_queue, skb_new);
	}

	/* FIN message : close socket */
	if (th->fin)
		sk->socket->state = SS_DISCONNECTING;

	wake_up(sk->sleep);

	return 0;
}