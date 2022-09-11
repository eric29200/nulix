#ifndef _SK_BUFF_H_
#define _SK_BUFF_H_

#include <lib/list.h>
#include <stddef.h>

/*
 * Socket buffer.
 */
struct sk_buff_t {
	struct net_device_t *		dev;
	struct ethernet_header_t *	eth_header;
	struct {
		struct icmp_header_t *	icmp_header;
		struct udp_header_t *	udp_header;
		struct tcp_header_t *	tcp_header;
	} h;
	struct {
		struct arp_header_t *	arp_header;
		struct ip_header_t *	ip_header;
	} nh;
	size_t				size;
	size_t				len;
	uint8_t *			head;
	uint8_t *			data;
	uint8_t *			tail;
	uint8_t *			end;
	struct list_head_t		list;
};

/*
 * Reserve some space in a socket buffer.
 */
static inline void skb_reserve(struct sk_buff_t *skb, size_t len)
{
	skb->data += len;
	skb->tail += len;
}

/*
 * Put data in a socket buffer.
 */
static inline uint8_t *skb_put(struct sk_buff_t *skb, size_t len)
{
	uint8_t *ret = skb->tail;
	skb->tail += len;
	skb->len += len;
	return ret;
}

/*
 * Push data into a socket buffer.
 */
static inline uint8_t *skb_push(struct sk_buff_t *skb, size_t len)
{
	skb->data -= len;
	skb->len += len;
	return skb->data;
}

/*
 * Pull data from a socket buffer.
 */
static inline uint8_t *skb_pull(struct sk_buff_t *skb, size_t len)
{
	skb->data += len;
	skb->len -= len;
	return skb->data;
}

struct sk_buff_t *skb_alloc(size_t size);
struct sk_buff_t *skb_clone(struct sk_buff_t *skb);
void skb_free(struct sk_buff_t *skb);

#endif
