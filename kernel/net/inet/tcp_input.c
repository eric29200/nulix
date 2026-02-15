#include <net/inet/tcp.h>
#include <net/sock.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Receive a packet in TCP_SYN_SENT state.
 */
int tcp_rcv_syn_sent(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_header *th = skb->h.tcp_header;
	struct tcp_opt *tp = &sk->protinfo.af_tcp;

	/* we only want to receive a SYN/ACK message */
	if (!th->ack)
		return 0;

	/* bad ack */
	if (TCP_SKB_CB(skb)->ack_seq != tp->snd_nxt)
		return 1;

	/* we only want to receive a SYN/ACK message */
	if (!th->syn)
		return 0;

	/* set connection established */
	tcp_set_state(sk, TCP_ESTABLISHED);

	/* ack packet */
	tcp_send_ack(sk, skb);

	return 0;
}

/*
 * Receive a packet in TCP_ESTABLISED state.
 */
static int tcp_rcv_established(struct sock *sk, struct sk_buff *skb)
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
	tcp_send_ack(sk, skb);

	/* clone socket buffer */
	skb_new = skb_clone(skb);
	if (!skb_new)
		return -ENOMEM;

	/* add buffer to socket */
	skb_queue_tail(&sk->receive_queue, skb_new);

	/* FIN message : go in CLOSE_WAIT state */
	if (th->fin)
	 	tcp_set_state(sk, TCP_CLOSE_WAIT);

	return 0;
}

/*
 * Receive a packet in TCP_FIN_WAIT1 state.
 */
static int tcp_rcv_fin_wait1(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_header *th = skb->h.tcp_header;
	struct tcp_opt *tp = &sk->protinfo.af_tcp;

	/* we wait for an ack */
	if (!th->ack || TCP_SKB_CB(skb)->ack_seq != tp->snd_nxt)
		return 1;

	/* ack packet */
	if (tcp_data_length(skb) || th->fin)
		tcp_send_ack(sk, skb);

	/* update socket state */
	sk->shutdown |= SEND_SHUTDOWN;
	tcp_set_state(sk, TCP_FIN_WAIT2);

	/* set timer to close socket */
	tcp_set_timer(sk, TCP_TIME_CLOSE, TCP_FIN_TIMEOUT);

	return 0;
}

/*
 * Receive a packet in TCP_FIN_WAIT2 state.
 */
static int tcp_rcv_fin_wait2(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_header *th = skb->h.tcp_header;
	struct tcp_opt *tp = &sk->protinfo.af_tcp;

	/* check sequence number */
	if (TCP_SKB_CB(skb)->ack_seq != tp->snd_nxt)
		return 1;

	/* ack packet if needed */
	if (th->fin)
		tcp_send_ack(sk, skb);

	/* update socket state */
	sk->shutdown |= SEND_SHUTDOWN;
	tcp_set_state(sk, TCP_TIME_WAIT);

	/* set timer to close socket */
	tcp_set_timer(sk, TCP_TIME_CLOSE, TCP_FIN_TIMEOUT);

	return 0;
}

/*
 * Receive a packet in TCP_LAST_ACK state.
 */
static int tcp_rcv_last_ack(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_header *th = skb->h.tcp_header;

	if (th->ack)
		tcp_set_state(sk, TCP_CLOSE);

	return 0;
}

/*
 * Receive a packet in non establised state.
 */
int tcp_rcv(struct sock *sk, struct sk_buff *skb)
{
	switch (sk->state) {
		case TCP_SYN_SENT:
			return tcp_rcv_syn_sent(sk, skb);
		case TCP_ESTABLISHED:
			return tcp_rcv_established(sk, skb);
		case TCP_FIN_WAIT1:
			return tcp_rcv_fin_wait1(sk, skb);
		case TCP_FIN_WAIT2:
			return tcp_rcv_fin_wait2(sk, skb);
		case TCP_LAST_ACK:
			return tcp_rcv_last_ack(sk, skb);
		case TCP_CLOSE:
			return 1;
		default:
			printf("tcp_rcv_state_process: unknown socket state %d\n", sk->state);
			break;
	}

	return 0;
}
