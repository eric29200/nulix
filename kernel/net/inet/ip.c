#include <net/inet/ip.h>
#include <net/inet/net.h>
#include <net/inet/udp.h>
#include <net/inet/icmp.h>
#include <net/inet/tcp.h>
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
	/* decode IP header */
	skb->nh.ip_header = (struct ip_header *) skb->data;
	skb_pull(skb, sizeof(struct ip_header));

	/* handle IPv4 only */
	if (skb->nh.ip_header->version != 4)
		return;

	/* check if packet is adressed to us */
	if (skb->dev->ip_addr != skb->nh.ip_header->dst_addr)
		return;

	/* go to next layer */
	switch (skb->nh.ip_header->protocol) {
		case IP_PROTO_UDP:
			udp_receive(skb);
			break;
		case IP_PROTO_ICMP:
			icmp_receive(skb);
			break;
		default:
			break;
	}

	/* deliver packet to sockets */
	net_deliver_skb(skb);
}

/*
 * Build and transmit an IP packet.
 */
int ip_build_xmit(struct sock *sk, void getfrag(const void *, char *, size_t), const void *frag, size_t size, uint32_t daddr)
{
	struct ip_header *iph;
	struct sk_buff *skb;
	struct route *rt;
	int ret;

	/* find route */
	rt = ip_rt_route(daddr);
	if (!rt)
		return -ENETUNREACH;

	/* add ip header to packet */
	size += sizeof(struct ip_header);

	/* allocate a socket buffer */
	skb = skb_alloc(rt->rt_dev->hard_header_len + size);
	if (!skb)
		return -ENOMEM;

	/* build hard header */
	rt->rt_dev->hard_header(skb, ETHERNET_TYPE_IP, rt->rt_dev->hw_addr, NULL);

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
	iph->src_addr = rt->rt_dev->ip_addr;
	iph->dst_addr = daddr;
	iph->chksum = net_checksum(iph, sizeof(struct ip_header));

	/* copy udp header + message */
	getfrag(frag, ((char *) iph) + sizeof(struct ip_header), size - sizeof(struct ip_header));

	/* rebuild hard header = find destination mac address */
	ret = rt->rt_dev->rebuild_header(rt->rt_dev, rt->rt_flags & RTF_GATEWAY ? rt->rt_gateway : daddr, skb);

	/* transmit message */
	if (ret == 0)
		net_transmit(rt->rt_dev, skb);

	/* on error, free socket buffer */
	if (ret < 0)
		skb_free(skb);

	return 0;
}