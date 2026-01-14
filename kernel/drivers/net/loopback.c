#include <drivers/net/loopback.h>
#include <net/inet/ethernet.h>
#include <net/inet/arp.h>
#include <net/inet/net.h>
#include <net/sk_buff.h>
#include <stderr.h>
#include <stdio.h>

/* Loopback device */
static struct net_device *loopback_dev = NULL;

/*
 * Send a socket buffer.
 */
static int loopback_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	/* set network device */
	skb->dev = loopback_dev;

	/* decode ethernet header */
	ethernet_receive(skb);

	/* handle socket buffer and free it */
	skb_handle(skb);
	skb_free(skb);

	/* update transmit and receive stats */
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->size;
	dev->stats.rx_packets++;
	dev->stats.rx_bytes += skb->size;

	return 0;
}

/*
 * Init loopback device.
 */
int init_loopback()
{
	/* register net device */
	loopback_dev = register_net_device(0, ARPHRD_ETHER, "lo");
	if (!loopback_dev)
		return -ENOSPC;

	/* set device */
	loopback_dev->family = AF_INET;
	loopback_dev->addr_len = ETHERNET_ALEN;
	loopback_dev->hard_header_len = ETHERNET_HLEN;
	loopback_dev->hard_header = ethernet_header;
	loopback_dev->rebuild_header = ethernet_rebuild_header;
	loopback_dev->start_xmit = loopback_start_xmit;
	loopback_dev->ip_addr = in_aton("127.0.0.1");
	loopback_dev->ip_netmask = in_aton("255.0.0.0");
	loopback_dev->flags = IFF_LOOPBACK;

	return 0;
}