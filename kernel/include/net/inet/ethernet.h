#ifndef _ETHERNET_H_
#define _ETHERNET_H_

#include <net/sk_buff.h>

#define ETHERNET_TYPE_ARP	0x0806
#define ETHERNET_TYPE_IP	0x0800

#define HARWARE_TYPE_ETHERNET	0x01

#define ETHERNET_MAX_MTU	0xFFFF

/*
 * Ethernet header.
 */
struct ethernet_header {
	uint8_t		dst_mac_addr[6];
	uint8_t		src_mac_addr[6];
	uint16_t	type;
};

void ethernet_receive(struct sk_buff *skb);
void ethernet_build_header(struct ethernet_header *eth_header, uint8_t *src_mac_addr, uint8_t *dst_mac_addr, uint16_t type);
int ethernet_rebuild_header(struct net_device *dev, uint32_t daddr, struct sk_buff *skb);

#endif
