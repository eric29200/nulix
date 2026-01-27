#include <net/sock.h>
#include <net/inet/tcp.h>
#include <net/inet/net.h>
#include <net/inet/ethernet.h>
#include <net/inet/route.h>
#include <proc/sched.h>
#include <fcntl.h>
#include <stderr.h>
#include <stdio.h>
#include <math.h>
#include <uio.h>

/*
 * TCP fake header.
 */
struct tcp_fake_header {
	struct tcp_header	th;
	uint32_t		saddr;
	uint32_t		daddr;
	struct iovec *		iov;
};

/*
 * Receive/decode a TCP packet.
 */
void tcp_receive(struct sk_buff *skb)
{
	/* decode TCP header */
	skb->h.tcp_header = (struct tcp_header *) skb->data;
	skb_pull(skb, sizeof(struct tcp_header));
}

/*
 * Handle a TCP packet.
 */
static int tcp_handle(struct sock *sk, struct sk_buff *skb)
{
	struct tcp_header *th = skb->h.tcp_header;

	/* check protocol, destination and source */
	if (sk->protocol != skb->nh.ip_header->protocol
		|| sk->sport != th->dst_port
		|| sk->dport != th->src_port)
		return -EINVAL;

	/* decode TCP header */
	TCP_SKB_CB(skb)->seq = ntohl(th->seq);
	TCP_SKB_CB(skb)->end_seq = TCP_SKB_CB(skb)->seq + th->syn + th->fin + tcp_data_length(skb);
	TCP_SKB_CB(skb)->ack_seq = ntohl(th->ack_seq);

	/* process socket buffer */
	switch (sk->state) {
		case TCP_ESTABLISHED:
			return tcp_rcv_established(sk, skb);
		default:
			return tcp_rcv_state_process(sk, skb);
	}

	return 0;
}

/*
 * Poll on TCP socket.
 */
static int tcp_poll(struct socket *sock, struct select_table *wait)
{
	struct sock *sk = sock->sk;
	int mask = 0;

	/* add wait queue to select table */
	select_wait(sk->sleep, wait);

	/* exceptional events ? */
	if (sk->err)
		mask = POLLERR;
	if (sk->shutdown & RCV_SHUTDOWN)
		mask |= POLLHUP;

	/* connected ? */
	if ((1 << sk->state) & ~(TCPF_SYN_SENT | TCPF_SYN_RECV)) {
		/* readable ? */
		if (!skb_queue_empty(&sk->receive_queue))
			mask |= POLLIN | POLLRDNORM;

		/* writable ? */
		if (!(sk->shutdown & SEND_SHUTDOWN) && sock_wspace(sk) >= MIN_WRITE_SPACE)
			mask |= POLLOUT | POLLWRNORM;
	}

	return mask;
}

/*
 * Receive a TCP message.
 */
static int tcp_recvmsg(struct sock *sk, struct msghdr *msg, size_t size)
{
	struct sockaddr_in *addr_in = (struct sockaddr_in *) msg->msg_name;
	size_t len, n, i, count = 0;
	struct sk_buff *skb;
	void *buf;

	/* unused size */
	UNUSED(size);

	/* sleep until we receive a packet */
	for (;;) {
		/* dead socket */
		if (sk->dead)
			return -ENOTCONN;

		/* signal received : restart system call */
		if (signal_pending(current_task))
			return -ERESTARTSYS;

		/* message received : break */
		if (!skb_queue_empty(&sk->receive_queue))
			break;

		/* non blocking */
		if (msg->msg_flags & MSG_DONTWAIT)
			return -EAGAIN;

		/* sleep */
		sleep_on(sk->sleep);
	}

	/* get first message */
	skb = skb_dequeue(&sk->receive_queue);

	/* get message */
	buf = tcp_data(skb) + sk->msg_position;
	len = tcp_data_length(skb) - sk->msg_position;

	/* copy message */
	for (i = 0; i < msg->msg_iovlen && len > 0; i++) {
		n = len > msg->msg_iov[i].iov_len ? msg->msg_iov[i].iov_len : len;
		memcpy(msg->msg_iov[i].iov_base, buf, n);
		count += n;
		len -= n;
		buf += n;
	}

	/* set source address */
	if (addr_in) {
		addr_in->sin_family = AF_INET;
		addr_in->sin_port = skb->h.tcp_header->src_port;
		addr_in->sin_addr = skb->nh.ip_header->src_addr;
	}

	/* free message or requeue it */
	if (!(msg->msg_flags & MSG_PEEK)) {
		if (len > 0) {
			sk->msg_position += count;
			skb_queue_head(&sk->receive_queue, skb);
		} else {
			sk->msg_position = 0;
			skb_free(skb);
		}
	} else {
		skb_queue_head(&sk->receive_queue, skb);
	}


	return count;
}

/*
 * Send a TCP message.
 */
static int tcp_sendmsg(struct sock *sk, const struct msghdr *msg, size_t size)
{
	int ret;

	/* sleep until connected */
	for (;;) {
		/* dead socket */
		if (sk->dead)
			return -ENOTCONN;

		/* signal received : restart system call */
		if (signal_pending(current_task))
			return -ERESTARTSYS;

		/* connected : break */
		if (tcp_connected(sk->state))
			break;

		/* non blocking */
		if (msg->msg_flags & MSG_DONTWAIT)
			return -EAGAIN;

		/* sleep */
		sleep_on(sk->sleep);
	}

	/* send message */
	ret = tcp_send_skb(sk, msg->msg_iov, size, TCPCB_FLAG_ACK);
	if (ret)
		return ret;

	return size;
}

/*
 * Create a TCP connection.
 */
static int tcp_connect(struct sock *sk, const struct sockaddr *addr, size_t addrlen)
{
	struct sockaddr_in *addr_in = (struct sockaddr_in *) addr;
	int ret;

	/* check address length */
	if (addrlen < sizeof(struct sockaddr_in))
		return -EINVAL;

	/* set destination */
	sk->daddr = addr_in->sin_addr;
	sk->dport = addr_in->sin_port;

	/* generate sequence */
	sk->protinfo.af_tcp.snd_nxt = ntohl(rand());
	sk->protinfo.af_tcp.rcv_nxt = 0;

	/* send SYN message */
	ret = tcp_send_skb(sk, NULL, 0, TCPCB_FLAG_SYN);
	if (ret)
		return ret;

	/* set socket state */
	sk->state = TCP_SYN_SENT;

	return 0;
}

/*
 * Switch to close state.
 */
static int tcp_close_state(struct sock *sk)
{
	int send_fin = 0;

	/* compute next state */
	switch (sk->state) {
		case TCP_SYN_SENT:		/* no SYN back, no FIN needed */
			sk->state = TCP_CLOSE;
			break;
		case TCP_SYN_RECV:
		case TCP_ESTABLISHED:		/* closedown begin */
			sk->state = TCP_FIN_WAIT1;
			send_fin = 1;
			break;
		case TCP_FIN_WAIT1:		/* already closing, or FIN sent : no change */
		case TCP_FIN_WAIT2:
		case TCP_CLOSING:
			break;
		case TCP_CLOSE:
		case TCP_LISTEN:
			sk->state = TCP_CLOSE;
			break;
		case TCP_CLOSE_WAIT:		/* they have FIN'd us. We send our FIN and wait only for the ACK */
			sk->state = TCP_LAST_ACK;
			send_fin = 1;
			break;
	}

	return send_fin;
}

/*
 * Close a TCP connection.
 */
static int tcp_close(struct sock *sk)
{
	struct sk_buff *skb;

	/* shutdown socket */
	sk->shutdown = SHUTDOWN_MASK;

	/* clear receive queue */
	while ((skb = skb_dequeue(&sk->receive_queue)) != NULL)
		skb_free(skb);

	/* switch to close state and send a FIN message if needed */
	if (tcp_close_state(sk))
		tcp_send_skb(sk, NULL, 0, TCPCB_FLAG_FIN | TCPCB_FLAG_ACK);

	/* set socket dead */
	sk->dead = 1;

	return 0;
}

/*
 * TCP get socket option.
 */
static int tcp_getsockopt(struct sock *sk, int level, int optname, void *optval, size_t *optlen)
{
	UNUSED(sk);
	UNUSED(level);
	UNUSED(optname);
	UNUSED(optval);
	UNUSED(optlen);
	printf("tcp_getsockopt: unknown option %d\n", optname);
	return 0;
}

/*
 * TCP set socket option.
 */
static int tcp_setsockopt(struct sock *sk, int level, int optname, void *optval, size_t optlen)
{
	UNUSED(sk);
	UNUSED(level);
	UNUSED(optname);
	UNUSED(optval);
	UNUSED(optlen);
	printf("tcp_setsockopt: unknown option %d\n", optname);
	return 0;
}

/*
 * TCP protocol.
 */
struct proto tcp_prot = {
	.handle		= tcp_handle,
	.close		= tcp_close,
	.recvmsg	= tcp_recvmsg,
	.sendmsg	= tcp_sendmsg,
	.connect	= tcp_connect,
	.poll		= tcp_poll,
	.getsockopt	= tcp_getsockopt,
	.setsockopt	= tcp_setsockopt,
};
