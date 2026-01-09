#include <net/sk_buff.h>
#include <net/sock.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <uio.h>
#include <string.h>
#include <stdio.h>

/*
 * Allocate a socket buffer.
 */
struct sk_buff *skb_alloc(size_t size)
{
	struct sk_buff *skb;

	/* allocate socket buffer */
	skb = (struct sk_buff *) kmalloc(sizeof(struct sk_buff) + size);
	if (!skb)
		return NULL;

	/* set socket buffer */
	memset(skb, 0, sizeof(struct sk_buff) + size);
	skb->data = (uint8_t *) skb + sizeof(struct sk_buff);
	skb->count = 1;
	skb->size = size;
	skb->head = skb->data;
	skb->tail = skb->data;
	skb->end = skb->data + size;
	skb->len = 0;

	return skb;
}

/*
 * Clone a socket buffer.
 */
struct sk_buff *skb_clone(struct sk_buff *skb)
{
	struct sk_buff *skb_new;

	/* allocate a new socket buffer */
	skb_new = (struct sk_buff *) kmalloc(sizeof(struct sk_buff));
	if (!skb_new)
		return NULL;

	/* link data */
	if (skb->data_skb)
		skb = skb->data_skb;
	skb->count++;

	/* set new socket buffer */
	memcpy(skb_new, skb, sizeof(struct sk_buff));
	skb_new->count = 1;
	skb_new->data_skb = skb;
	skb_new->next = NULL;
	skb_new->prev = NULL;
	skb_new->list = NULL;
	skb_new->sk = NULL;

	return skb_new;
}

/*
 * Free memory of a socket buffer.
 */
static void __skb_freemem(struct sk_buff *skb)
{
	/* still used */
	if (--skb->count)
		return;

	kfree(skb->head);
}

/*
 * Free memory of a socket buffer.
 */
void skb_freemem(struct sk_buff *skb)
{
	void *addr = skb->head;

	/* still used */
	if (--skb->count)
		return;

	/* free socket buffer containing data */
	if (skb->data_skb) {
		addr = skb;
		__skb_freemem(skb->data_skb);
	}

	kfree(addr);
}

/*
 * Free a socket buffer.
 */
void skb_free(struct sk_buff *skb)
{
	/* decrease allocated memory */
	if (skb->sk && skb->sk->wmem_alloc > 0) {
		if (skb->sk->wmem_alloc > skb->size)
			skb->sk->wmem_alloc -= skb->size;
		else
			skb->sk->wmem_alloc = 0;

		/* wake up writers */
		wake_up(skb->sk->sleep);
	}

	/* free socket buffer */
	skb_freemem(skb);
}

/*
 * Copy data from iovec.
 */
void memcpy_fromiovec(uint8_t *data, struct iovec *iov, size_t len)
{
	size_t copy;

	while (len > 0) {
		if (iov->iov_len) {
			copy = len <= iov->iov_len ? len : iov->iov_len;
			memcpy(data, iov->iov_base, copy);
			len -= copy;
			data += copy;
			iov->iov_base += copy;
			iov->iov_len -= copy;
		}

		iov++;
	}
}

/*
 * Copy data to iovec.
 */
void memcpy_toiovec(struct iovec *iov, uint8_t *data, size_t len)
{
	size_t copy;

	while (len > 0) {
		if (iov->iov_len) {
			copy = len <= iov->iov_len ? len : iov->iov_len;
			memcpy(iov->iov_base, data, copy);
			len -= copy;
			data += copy;
			iov->iov_base += copy;
			iov->iov_len -= copy;
		}

		iov++;
	}
}
