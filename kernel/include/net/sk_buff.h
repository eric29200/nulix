#ifndef _SK_BUFF_H_
#define _SK_BUFF_H_

#include <net/socket.h>
#include <lib/list.h>
#include <uio.h>
#include <stddef.h>

#define SK_WMEM_MAX			65536
#define SK_RMEM_MAX			65536

/*
 * Socket buffer.
 */
struct sk_buff {
	struct sk_buff *			next;
	struct sk_buff *			prev;
	struct sk_buff_head *			list;
	struct sock *				sk;
	struct net_device *			dev;
	int					count;
	uint32_t				raddr;
	uint8_t					arp;
	uint8_t					free;
	struct sk_buff *			data_skb;
	union {
		struct ethernet_header *	eth_header;
		uint8_t *			raw;
	} hh;
	union {
		struct icmp_header *		icmp_header;
		struct udp_header *		udp_header;
		struct tcp_header *		tcp_header;
		uint8_t *			raw;
	} h;
	union {
		struct arp_header *		arp_header;
		struct ip_header *		ip_header;
		uint8_t *			raw;
	} nh;
	size_t					size;
	size_t					len;
	uint8_t *				head;
	uint8_t *				data;
	uint8_t *				tail;
	uint8_t *				end;
	char					cb[48];
};

/*
 * Socket buffer queue.
 */
struct sk_buff_head {
	struct sk_buff *			next;
	struct sk_buff *			prev;
	size_t					len;
};

/*
 * Init a socket buffer queue.
 */
static inline void skb_queue_head_init(struct sk_buff_head *list)
{
	list->prev = (struct sk_buff *) list;
	list->next = (struct sk_buff *) list;
	list->len = 0;
}

/*
 * Get length of a socket buffer queue.
 */
static inline size_t skb_queue_len(struct sk_buff_head *list)
{
	return list->len;
}

/*
 * Is a socket buffer queue empty ?
 */
static inline int skb_queue_empty(struct sk_buff_head *list)
{
	return list->next == (struct sk_buff *) list;
}

/*
 * Peek next socket buffer.
 */
static inline struct sk_buff *skb_peek(struct sk_buff_head *list_)
{
	struct sk_buff *list = ((struct sk_buff *) list_)->next;
	if (list == (struct sk_buff *) list_)
		list = NULL;
	return list;
}

/*
 * Dequeue next socket buffer.
 */
static inline struct sk_buff *skb_dequeue(struct sk_buff_head *list)
{
	struct sk_buff *next, *prev, *res;

	prev = (struct sk_buff *) list;
	next = prev->next;
	if (next == prev)
		return NULL;

	list->len--;
	res = next;
	next = next->next;
	next->prev = prev;
	prev->next = next;
	res->next = NULL;
	res->prev = NULL;
	res->list = NULL;

	return res;
}

/*
 * Unlink a socket buffer.
 */
static inline void skb_unlink(struct sk_buff *skb)
{
	struct sk_buff *next, *prev;

	if (skb->list) {
		skb->list->len--;
		next = skb->next;
		prev = skb->prev;
		skb->next = NULL;
		skb->prev = NULL;
		next->prev = prev;
		prev->next = next;
	}
}

/*
 * Insert a socket buffer at the end of a queue.
 */
static inline void skb_queue_tail(struct sk_buff_head *list, struct sk_buff *newsk)
{
	struct sk_buff *prev, *next;

	list->len++;
	newsk->list = list;
	next = (struct sk_buff *)list;
	prev = next->prev;
	newsk->next = next;
	newsk->prev = prev;
	next->prev = newsk;
	prev->next = newsk;
}

/*
 * Insert a socket buffer at the beginning of a queue.
 */
static inline void skb_queue_head(struct sk_buff_head *list, struct sk_buff *newsk)
{
	struct sk_buff *prev, *next;

	list->len++;
	newsk->list = list;
	prev = (struct sk_buff *)list;
	next = prev->next;
	newsk->next = next;
	newsk->prev = prev;
	next->prev = newsk;
	prev->next = newsk;
}

/*
 * Reserve some space in a socket buffer.
 */
static inline void skb_reserve(struct sk_buff *skb, size_t len)
{
	skb->data += len;
	skb->tail += len;
}

/*
 * Put data in a socket buffer.
 */
static inline uint8_t *skb_put(struct sk_buff *skb, size_t len)
{
	uint8_t *ret = skb->tail;
	skb->tail += len;
	skb->len += len;
	return ret;
}

/*
 * Push data into a socket buffer.
 */
static inline uint8_t *skb_push(struct sk_buff *skb, size_t len)
{
	skb->data -= len;
	skb->len += len;
	return skb->data;
}

/*
 * Pull data from a socket buffer.
 */
static inline uint8_t *skb_pull(struct sk_buff *skb, size_t len)
{
	skb->data += len;
	skb->len -= len;
	return skb->data;
}

/*
 * Get tail room.
 */
static inline int skb_tailroom(struct sk_buff *skb)
{
	return skb->end - skb->tail;
}

struct sk_buff *skb_alloc(size_t size);
struct sk_buff *skb_clone(struct sk_buff *skb);
void skb_free(struct sk_buff *skb);
void memcpy_fromiovec(uint8_t *data, struct iovec *iov, size_t len);
void memcpy_toiovec(struct iovec *iov, uint8_t *data, size_t len);

#endif
