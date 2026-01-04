#ifndef _NET_H_
#define _NET_H_

#include <net/sk_buff.h>
#include <net/if.h>
#include <proc/timer.h>

#define NR_NET_DEVICES			4

#define htons(s)			((((s) & 0xFF) << 8) | (((s) & 0xFF00) >> 8))
#define htonl(l)			((((l) & 0xFF) << 24) | (((l) & 0xFF00) << 8) | (((l) & 0xFF0000) >> 8) | (((l) & 0xFF000000) >> 24))
#define ntohs(s)			(htons((s)))
#define ntohl(s)			(htonl((s)))

/*
 * Network device.
 */
struct net_device {
	char *			name;
	uint8_t			index;
	uint32_t		io_base;
	uint8_t			irq;
	uint8_t			mac_addr[6];
	uint8_t			ip_addr[4];
	uint8_t			ip_netmask[4];
	uint8_t			ip_route[4];
	uint16_t		type;
	uint16_t		flags;
	void *			private;
	int			(*start_xmit)(struct sk_buff *, struct net_device *);
};

/* network devices */
extern struct net_device net_devices[NR_NET_DEVICES];
extern int nr_net_devices;

/* network prototypes */
int init_net_dev();;
struct net_device *register_net_device(uint32_t io_base, uint16_t type);
struct net_device *net_device_find(const char *name);
int dev_ioctl(unsigned int cmd, void *arg);
void skb_handle(struct sk_buff *skb);
uint16_t net_checksum(void *data, size_t size);
void net_handle(struct sk_buff *skb);
void net_transmit(struct net_device *net_dev, struct sk_buff *skb);
void net_deliver_skb(struct sk_buff *skb);

#endif
