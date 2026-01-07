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
	memcpy(eth_header->src_mac_addr, src_mac_addr, 6);
	if (dst_mac_addr)
		memcpy(eth_header->dst_mac_addr, dst_mac_addr, 6);
	eth_header->type = htons(type);
}

/*
 * Rebuild an ethernet header (find destination MAC address).
 */
int ethernet_rebuild_header(struct net_device *dev, struct sk_buff *skb)
{
	struct arp_table_entry *arp_entry;
	uint32_t route_ip;

	/* ARP request : do not rebuild header */
	if (skb->eth_header->type == htons(ETHERNET_TYPE_ARP))
		return 0;

	/* get route IP */
	route_ip = ip_route(dev, skb->nh.ip_header->dst_addr);

	/* find ARP entry */
	arp_entry = arp_lookup(dev, route_ip, 0);
	if (!arp_entry)
		return -EINVAL;

	/* set MAC address */
	memcpy(skb->eth_header->dst_mac_addr, arp_entry->mac_addr, 6);

	return 0;
}

/*
 * Receive/decode an ethernet packet.
 */
void ethernet_receive(struct sk_buff *skb)
{
	skb->eth_header = (struct ethernet_header *) skb->data;
	skb_pull(skb, sizeof(struct ethernet_header));
}
