#include <net/inet/ethernet.h>
#include <net/inet/net.h>
#include <net/inet/ip.h>
#include <net/inet/arp.h>
#include <stderr.h>
#include <string.h>

/*
 * Build an ethernet header.
 */
void ethernet_build_header(struct ethernet_header *eth_header, uint8_t *src_hw_addr, uint8_t *dst_hw_addr, uint16_t type)
{
	eth_header->type = htons(type);

	/* copy source address */
	memcpy(eth_header->src_hw_addr, src_hw_addr, ETHERNET_ALEN);

	/* copy destination address */
	if (dst_hw_addr)
		memcpy(eth_header->dst_hw_addr, dst_hw_addr, ETHERNET_ALEN);
}

/*
 * Rebuild ethernet header = find destination address.
 */
int ethernet_rebuild_header(struct net_device *dev, uint32_t daddr, struct sk_buff *skb)
{
	return arp_find(dev, skb->hh.eth_header->dst_hw_addr, daddr, skb);
}

/*
 * Receive/decode an ethernet packet.
 */
void ethernet_receive(struct sk_buff *skb)
{
	skb->hh.eth_header = (struct ethernet_header *) skb->data;
	skb_pull(skb, sizeof(struct ethernet_header));
}
