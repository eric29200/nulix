#include <net/inet/tcp.h>
#include <net/sock.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Network timer to handle delayed operations.
 */
static void net_timer(void *arg)
{
	struct sock *sk = (struct sock *) arg;
	int why = sk->timeout;

	switch (why) {
		case TCP_TIME_DONE:
			/* if the socket hasn't been closed off, re-try a bit later */
			if (!sk->dead) {
				tcp_set_timer(sk, TCP_TIME_DONE, TCP_DONE_TIME);
				break;
			}

			/* destroy socket */
			destroy_sock(sk);
			break;
		case TCP_TIME_CLOSE:
			sk->shutdown = SHUTDOWN_MASK;
			tcp_set_state(sk, TCP_CLOSE);
			break;
		default:
			printf("net_timer: timer expired - reason %d is unknown\n", why);
			break;
	}
}

/*
 * Set a timer on a socket.
 */
void tcp_set_timer(struct sock *sk, int timeout, time_t expires)
{
	/* delete previous timer */
	del_timer(&sk->timer);

	/* set new timer */
	sk->timeout = timeout;
	init_timer(&sk->timer, net_timer, sk, jiffies + expires);
	add_timer(&sk->timer);
}