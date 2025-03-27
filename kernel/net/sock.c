#include <net/sock.h>
#include <net/sk_buff.h>
#include <proc/sched.h>
#include <stderr.h>
#include <stdio.h>
  
/*
 * Allocate a send buffer.
 */
struct sk_buff *sock_alloc_send_skb(struct socket *sock, size_t len)
{
	struct sk_buff *skb;

	/* allocate a socket buffer */
	skb = skb_alloc(len);
	if (!skb)
		return NULL;

	/* set socket */
	skb->sock = sock;

	return skb;
}