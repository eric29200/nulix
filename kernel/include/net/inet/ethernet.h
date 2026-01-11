#ifndef _ETHERNET_H_
#define _ETHERNET_H_

#include <net/sk_buff.h>

#define ETHERNET_TYPE_ARP	0x0806
#define ETHERNET_TYPE_IP	0x0800

#define ETHERNET_ALEN		6
#define ETHERNET_HLEN		14

#define HARDWARE_TYPE_ETHERNET	0x01

#define ETHERNET_MAX_MTU	0xFFFF

/*
 * Ethernet header.
 */
struct ethernet_header {
	uint8_t			dst_hw_addr[ETHERNET_ALEN];
	uint8_t			src_hw_addr[ETHERNET_ALEN];
	uint16_t		type;
} __attribute__ ((packed));

void ethernet_receive(struct sk_buff *skb);
void ethernet_header(struct sk_buff *skb, uint16_t type, uint8_t *saddr, uint8_t *daddr);
int ethernet_rebuild_header(struct net_device *dev, uint32_t daddr, struct sk_buff *skb);

#endif
