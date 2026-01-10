#include <net/inet/ethernet.h>
#include <net/inet/net.h>
#include <net/inet/ip.h>
#include <net/inet/arp.h>
#include <stderr.h>
#include <string.h>

/*
 * Build an ethernet header.
 */
void ethernet_build_header(struct ethernet_header *eth_header, uint8_t *src_mac_addr, uint8_t *dst_mac_addr, uint16_t type)
{
	eth_header->type = htons(type);

	/* copy source address */
	memcpy(eth_header->src_mac_addr, src_mac_addr, 6);

	/* copy destination address */
	if (dst_mac_addr)
		memcpy(eth_header->dst_mac_addr, dst_mac_addr, 6);
}

/*
 * Rebuild ethernet header = find destination address.
 */
void ethernet_rebuild_header(struct ethernet_header *eth_header, struct net_device *dev, uint32_t daddr)
{
	struct arp_table_entry *arp_entry;

	/* find destination address */
	arp_entry = arp_lookup(dev, daddr, 1);

	/* copy destination address */
	memcpy(eth_header->dst_mac_addr, arp_entry->mac_addr, 6);
}

/*
 * Receive/decode an ethernet packet.
 */
void ethernet_receive(struct sk_buff *skb)
{
	skb->hh.eth_header = (struct ethernet_header *) skb->data;
	skb_pull(skb, sizeof(struct ethernet_header));
}
