#include <net/sock.h>
#include <net/inet/icmp.h>
#include <net/inet/net.h>
#include <net/inet/ethernet.h>
#include <net/inet/ip.h>
#include <proc/sched.h>
#include <uio.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>

/*
 * Receive/decode an ICMP packet.
 */
void icmp_receive(struct sk_buff *skb)
{
	skb->h.icmp_header = (struct icmp_header *) skb->data;
	skb_pull(skb, sizeof(struct icmp_header));

	/* recompute checksum */
	skb->h.icmp_header->chksum = 0;
	skb->h.icmp_header->chksum = net_checksum(skb->h.icmp_header, skb->size - sizeof(struct ethernet_header) - sizeof(struct ip_header));
}