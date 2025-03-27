#include <net/sk_buff.h>
#include <net/sock.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <uio.h>
#include <string.h>

/*
 * Allocate a socket buffer.
 */
struct sk_buff *skb_alloc(size_t size)
{
	struct sk_buff *skb;
	uint8_t *data;

	/* allocate socket buffer */
	skb = (struct sk_buff *) kmalloc(sizeof(struct sk_buff));
	if (!skb)
		return NULL;
	memset(skb, 0, sizeof(struct sk_buff));

	/* allocate data */
	skb->size = size;
	data = (uint8_t *) kmalloc(size);
	if (!data) {
		kfree(skb);
		return NULL;
	}

	/* set socket buffer */
	memset(data, 0, size);
	skb->head = data;
	skb->data = data;
	skb->tail = data;
	skb->end = data + size;
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
	skb_new = skb_alloc(skb->size);
	if (!skb_new)
		return NULL;

	/* put data */
	if (skb->len > 0) {
		memcpy(skb_new->head, skb->head, skb->size);
		skb_put(skb_new, skb->len);
	}

	return skb_new;
}

/*
 * Free a socket buffer.
 */
void skb_free(struct sk_buff *skb)
{
	kfree(skb->head);
	kfree(skb);
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