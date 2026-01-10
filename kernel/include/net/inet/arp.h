#ifndef _ARP_H_
#define _ARP_H_

#include <net/sk_buff.h>

#define ARP_REQUEST		1
#define ARP_REPLY		2

#define ARPHRD_ETHER 		1

/*
 * ARP header.
 */
struct arp_header {
	uint16_t	hardware_type;
	uint16_t	protocol;
	uint8_t		hardware_addr_len;
	uint8_t		protocol_addr_len;
	uint16_t	opcode;
	uint8_t		src_hardware_addr[6];
	uint32_t	src_protocol_addr;
	uint8_t		dst_hardware_addr[6];
	uint32_t	dst_protocol_addr;
} __attribute__ ((packed));

void arp_receive(struct sk_buff *skb);
int arp_find(struct net_device *dev, uint32_t ip_addr, uint8_t *mac_addr, struct sk_buff *skb);

#endif
