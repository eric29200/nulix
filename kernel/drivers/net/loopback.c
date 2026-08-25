#include <drivers/net/loopback.h>
#include <net/inet/ethernet.h>
#include <net/inet/arp.h>
#include <net/inet/net.h>
#include <net/sk_buff.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Send a socket buffer.
 */
static int loopback_start_xmit(struct sk_buff *skb, struct net_device *net_dev)
{
	/* set network device */
	skb->dev = net_dev;

	/* decode ethernet header */
	ethernet_receive(skb);

	/* update transmit and receive stats */
	net_dev->stats.tx_packets++;
	net_dev->stats.tx_bytes += skb->size;
	net_dev->stats.rx_packets++;
	net_dev->stats.rx_bytes += skb->size;

	/* receive packet */
	netif_rx(skb);

	return 0;
}

/*
 * Init loopback device.
 */
int init_loopback()
{
	struct net_device *net_dev;

	/* register net device */
	net_dev = register_net_device(0, ARPHRD_ETHER, AF_INET, "lo");
	if (!net_dev)
		return -ENOSPC;

	/* set device */
	net_dev->addr_len = ETHERNET_ALEN;
	net_dev->hard_header_len = ETHERNET_HLEN;
	net_dev->hard_header = ethernet_header;
	net_dev->rebuild_header = ethernet_rebuild_header;
	net_dev->start_xmit = loopback_start_xmit;
	net_dev->ip_addr = in_aton("127.0.0.1");
	net_dev->ip_netmask = in_aton("255.0.0.0");
	net_dev->flags = IFF_LOOPBACK;

	return 0;
}
