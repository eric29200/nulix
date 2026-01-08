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
 * Receive/decode an ethernet packet.
 */
void ethernet_receive(struct sk_buff *skb)
{
	skb->hh.eth_header = (struct ethernet_header *) skb->data;
	skb_pull(skb, sizeof(struct ethernet_header));
}
