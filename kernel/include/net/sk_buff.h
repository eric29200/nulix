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
	struct net_device *		dev;
	struct ethernet_header *	eth_header;
	struct {
		struct icmp_header *	icmp_header;
		struct udp_header *	udp_header;
		struct tcp_header *	tcp_header;
	} h;
	struct {
		struct arp_header *	arp_header;
		struct ip_header *	ip_header;
	} nh;
	size_t				size;
	size_t				len;
	uint8_t *			head;
	uint8_t *			data;
	uint8_t *			tail;
	uint8_t *			end;
	struct socket *			sock;
	struct list_head		list;
};

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

struct sk_buff *skb_alloc(size_t size);
struct sk_buff *skb_clone(struct sk_buff *skb);
void skb_free(struct sk_buff *skb);
void memcpy_fromiovec(uint8_t *data, struct iovec *iov, size_t len);
void memcpy_toiovec(struct iovec *iov, uint8_t *data, size_t len);

#endif
