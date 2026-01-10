#include <net/inet/ip.h>
#include <net/inet/net.h>
#include <net/inet/ethernet.h>
#include <net/inet/route.h>
#include <net/inet/arp.h>
#include <net/sock.h>
#include <string.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Receive/decode an IP packet.
 */
void ip_receive(struct sk_buff *skb)
{
	skb->nh.ip_header = (struct ip_header *) skb->data;
	skb_pull(skb, sizeof(struct ip_header));
}

/*
 * Build and transmit an IP packet.
 */
int ip_build_xmit(struct sock *sk, void getfrag(const void *, char *, size_t), const void *frag, size_t size, uint32_t daddr)
{
	struct arp_table_entry *arp_entry;
	struct ip_header *iph;
	struct sk_buff *skb;
	struct route *rt;

	/* find route */
	rt = ip_rt_route(daddr);
	if (!rt)
		return -ENETUNREACH;

	/* find destination mac address */
	arp_entry = arp_lookup(rt->rt_dev, rt->rt_flags & RTF_GATEWAY ? rt->rt_gateway : daddr, 1);
	if (!arp_entry)
		return -ENETUNREACH;

	/* add ip header to packet */
	size += sizeof(struct ip_header);

	/* allocate a socket buffer */
	skb = skb_alloc(sizeof(struct ethernet_header) + size);
	if (!skb)
		return -ENOMEM;

	/* build ethernet header */
	skb->hh.eth_header = (struct ethernet_header *) skb_put(skb, sizeof(struct ethernet_header));
	ethernet_build_header(skb->hh.eth_header, sk->protinfo.af_inet.dev->mac_addr, arp_entry->mac_addr, ETHERNET_TYPE_IP);

	/* build ip header */
	skb->nh.ip_header = iph = (struct ip_header *) skb_put(skb, size);
	iph->ihl = 5;
	iph->version = 4;
	iph->tos = 0;
	iph->length = htons(size);
	iph->id = htons(0);
	iph->fragment_offset = 0;
	iph->ttl = IPV4_DEFAULT_TTL;
	iph->protocol = sk->protocol;
	iph->src_addr = sk->protinfo.af_inet.dev->ip_addr;
	iph->dst_addr = daddr;
	iph->chksum = net_checksum(iph, sizeof(struct ip_header));

	/* copy udp header + message */
	getfrag(frag, ((char *) iph) + sizeof(struct ip_header), size - sizeof(struct ip_header));

	/* transmit message */
	net_transmit(sk->protinfo.af_inet.dev, skb);

	return 0;
}