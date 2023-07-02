#ifndef _NET_H_
#define _NET_H_

#include <net/sk_buff.h>

#define NR_NET_DEVICES			4
#define NET_HANDLE_FREQ_MS		10

#define htons(s)			((((s) & 0xFF) << 8) | (((s) & 0xFF00) >> 8))
#define htonl(l)			((((l) & 0xFF) << 24) | (((l) & 0xFF00) << 8) | (((l) & 0xFF0000) >> 8) | (((l) & 0xFF000000) >> 24))
#define ntohs(s)			(htons((s)))
#define ntohl(s)			(htonl((s)))

/*
 * Network device.
 */
struct net_device_t {
	char *			name;
	uint8_t			index;
	uint32_t		io_base;
	uint8_t			irq;
	uint8_t			mac_addr[6];
	uint8_t			ip_addr[4];
	uint8_t			ip_netmask[4];
	uint8_t			ip_route[4];
	struct task_t *		thread;
	struct wait_queue_t *	wait;
	struct list_head_t	skb_input_list;
	struct list_head_t	skb_output_list;
	void			(*send_packet)(struct sk_buff_t *);
};

/* network devices */
extern struct net_device_t net_devices[NR_NET_DEVICES];
extern int nr_net_devices;

/* network prototypes */
struct net_device_t *register_net_device(uint32_t io_base);
struct net_device_t *net_device_find(const char *name);
void skb_handle(struct sk_buff_t *skb);
uint16_t net_checksum(void *data, size_t size);
void net_handle(struct net_device_t *net_dev, struct sk_buff_t *skb);
void net_transmit(struct net_device_t *net_dev, struct sk_buff_t *skb);
void net_deliver_skb(struct sk_buff_t *skb);

#endif