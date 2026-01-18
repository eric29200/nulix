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
 * Check an IP address.
 */
int ip_chk_addr(uint32_t addr)
{
	struct net_device *dev;
	int i;

	/* broadcast address */
	if (addr == INADDR_ANY || addr == INADDR_BROADCAST || addr == htonl(0x7FFFFFFFL))
		return IS_BROADCAST;

	/* our address */
	if ((addr & htonl(0x7F000000L)) == htonl(0x7F000000L))
		return IS_MYADDR;

	/* find network device */
	for (i = 0; i < nr_net_devices; i++) {
		dev = &net_devices[i];

		/* skip down devices */
		if ((!(dev->flags & IFF_UP)) || dev->family != AF_INET)
			continue;

		/* exact address */
		if (addr == dev->ip_addr)
			return IS_MYADDR;
	}

	return 0;
}

/*
 * Get loopback address.
 */
uint32_t ip_my_addr()
{
  	struct net_device *dev;
	int i;

	for (i = 0; i < nr_net_devices; i++) {
		dev = &net_devices[i];

		if (dev->flags & IFF_LOOPBACK)
			return dev->ip_addr;
  	}

	return 0;
}

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
		case IP_PROTO_TCP:
			tcp_receive(skb);
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
 * Build IP header.
 */
int ip_build_header(struct sk_buff *skb, uint32_t daddr, size_t size, struct net_device **dev)
{
	struct ip_header *iph;
	struct route *rt;

	/* find route */
	rt = ip_rt_route(daddr);
	if (!rt)
		return -ENETUNREACH;

	/* build hard header */
	*dev = rt->rt_dev;
	rt->rt_dev->hard_header(skb, ETHERNET_TYPE_IP, rt->rt_dev->hw_addr, NULL);

	/* build ip header */
	skb->nh.ip_header = iph = (struct ip_header *) skb_put(skb, sizeof(struct ip_header));
	iph->ihl = 5;
	iph->version = 4;
	iph->tos = 0;
	iph->length = htons(size + sizeof(struct ip_header));
	iph->id = htons(0);
	iph->fragment_offset = 0;
	iph->ttl = IPV4_DEFAULT_TTL;
	iph->protocol = skb->sk->protocol;
	iph->src_addr = rt->rt_dev->ip_addr;
	iph->dst_addr = daddr;
	iph->chksum = net_checksum(iph, sizeof(struct ip_header));

	/* rebuild hard header = find destination mac address */
	return rt->rt_dev->rebuild_header(rt->rt_dev, rt->rt_flags & RTF_GATEWAY ? rt->rt_gateway : daddr, skb);
}

/*
 * Build and transmit an IP packet.
 */
int ip_build_xmit(struct sock *sk, void getfrag(const void *, char *, size_t), const void *frag, size_t size, uint32_t daddr, int flags)
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
	skb = sock_alloc_send_skb(sk, rt->rt_dev->hard_header_len + size, flags & MSG_DONTWAIT, &ret);
	if (!skb)
		return ret;

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