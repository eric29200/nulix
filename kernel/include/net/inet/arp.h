#ifndef _ARP_H_
#define _ARP_H_

#include <net/sk_buff.h>

#define ARP_REQUEST		1
#define ARP_REPLY		2

#define ARPHRD_ETHER 		1

/* ARP flag values */
#define ATF_COM			0x02		/* completed entry (ha valid) */
#define	ATF_PERM		0x04		/* permanent entry */
#define	ATF_PUBL		0x08		/* publish entry */
#define	ATF_USETRAILERS		0x10		/* has requested trailers */
#define ATF_NETMASK     	0x20            /* want to use a netmask (only for proxy entries) */


/*
 * ARP header.
 */
struct arp_header {
	uint16_t		hw_type;
	uint16_t		protocol;
	uint8_t			hw_addr_len;
	uint8_t			prot_addr_len;
	uint16_t		opcode;
} __attribute__ ((packed));

void arp_receive(struct sk_buff *skb);
int arp_find(struct net_device *dev, uint8_t *hw_addr, uint32_t ip_addr, struct sk_buff *skb);

#endif
