#include <net/inet/tcp.h>
#include <net/sock.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <math.h>

/* inet sockets */
extern struct list_head inet_sockets;

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
 * Receive a packet in TCP_SYN_RECV state.
 */
int tcp_rcv_syn_recv(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_header *th = skb->h.tcp_header;
	struct tcp_opt *tp = &sk->protinfo.af_tcp;

	/* we wait for the ack */
	if (!th->ack || TCP_SKB_CB(skb)->ack_seq != tp->snd_nxt)
		return 1;

	/* set connection established */
	tcp_set_state(sk, TCP_ESTABLISHED);

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
	if (!data_len && !th->fin)
		return 0;

	/* send ACK message */
	tcp_send_ack(sk, skb);

	/* handle data */
	if (data_len > 0) {
		/* clone socket buffer */
		skb_new = skb_clone(skb);
		if (!skb_new)
			return -ENOMEM;

		/* add buffer to socket */
		skb_queue_tail(&sk->receive_queue, skb_new);
	}

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
 * Receive a packet in TCP_LISTEN state.
 */
static int tcp_rcv_listen(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_header *th = skb->h.tcp_header;
	struct tcp_opt *tp_new;
	struct sock *sk_new;

	/* we wait for a SYN message */
	if (th->ack || th->rst || !th->syn)
		return 1;

	/* check socket */
	if (sk->dead || sk->ack_backlog >= sk->max_ack_backlog)
		return 1;

	/* open a new connection */
	sk_new = sk_alloc(sk->family, 0);
	if (!sk_new)
		return 1;

	/* set new socket */
	memcpy(sk_new, sk, sizeof(struct sock));
	skb_queue_head_init(&sk_new->receive_queue);
	sk_new->proc = 0;
	sk_new->pair = NULL;
	sk_new->wmem_alloc = 0;
	sk_new->err = 0;
	sk_new->shutdown = 0;
	sk_new->ack_backlog = 0;
	sk_new->state = TCP_SYN_RECV;
	sk_new->timeout = 0;
	sk_new->socket = NULL;

	/* set tcp options */
	tp_new = &sk_new->protinfo.af_tcp;
	tp_new->iss = ntohl(rand());
	tp_new->snd_nxt = tp_new->iss;
	tp_new->rcv_nxt = TCP_SKB_CB(skb)->seq + 1;

	/* set addresses */
	sk_new->daddr = skb->nh.ip_header->src_addr;
	sk_new->dport = skb->h.tcp_header->src_port;
	sk_new->saddr = skb->nh.ip_header->dst_addr;
	sk_new->rcv_saddr = skb->nh.ip_header->dst_addr;
	sk_new->sport = skb->h.tcp_header->dst_port;

	/* insert in sockets list */
	list_del(&sk_new->list);
	list_add_tail(&sk_new->list, &inet_sockets);

	/* send SYN/ACK */
	tcp_send_syn_ack(sk_new, sk);

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
		case TCP_SYN_RECV:
			return tcp_rcv_syn_recv(sk, skb);
		case TCP_ESTABLISHED:
			return tcp_rcv_established(sk, skb);
		case TCP_FIN_WAIT1:
			return tcp_rcv_fin_wait1(sk, skb);
		case TCP_FIN_WAIT2:
			return tcp_rcv_fin_wait2(sk, skb);
		case TCP_LAST_ACK:
			return tcp_rcv_last_ack(sk, skb);
		case TCP_LISTEN:
			return tcp_rcv_listen(sk, skb);
		case TCP_CLOSE:
			return 1;
		default:
			printf("tcp_rcv_state_process: unknown socket state %d\n", sk->state);
			break;
	}

	return 0;
}
