#include <net/inet/ip.h>
#include <net/inet/net.h>
#include <string.h>
#include <stdio.h>

/*
 * Build an IPv4 header.
 */
void ip_build_header(struct ip_header *ip_header, uint8_t tos, uint16_t length, uint16_t id,
		     uint8_t ttl, uint8_t protocol, uint32_t src_addr, uint32_t dst_addr)
{
	ip_header->ihl = 5;
	ip_header->version = 4;
	ip_header->tos = tos;
	ip_header->length = htons(length);
	ip_header->id = htons(id);
	ip_header->fragment_offset = 0;
	ip_header->ttl = ttl;
	ip_header->protocol = protocol;
	ip_header->src_addr = src_addr;
	ip_header->dst_addr = dst_addr;
	ip_header->chksum = net_checksum(ip_header, sizeof(struct ip_header));
}

/*
 * Receive/decode an IP packet.
 */
void ip_receive(struct sk_buff *skb)
{
	skb->nh.ip_header = (struct ip_header *) skb->data;
	skb_pull(skb, sizeof(struct ip_header));
}

/*
 * Get IP route.
 */
uint32_t ip_route(struct net_device *dev, const uint32_t dest_ip)
{
	return (dev->ip_addr & dev->ip_netmask) == (dest_ip & dev->ip_netmask)
		? dest_ip
		: dev->ip_route;
}

