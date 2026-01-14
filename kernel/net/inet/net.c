#include <net/sock.h>
#include <net/inet/net.h>
#include <net/inet/ethernet.h>
#include <net/inet/arp.h>
#include <net/inet/ip.h>
#include <net/inet/icmp.h>
#include <net/inet/udp.h>
#include <net/inet/tcp.h>
#include <net/if.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>

/* network devices */
struct net_device net_devices[NR_NET_DEVICES];
int nr_net_devices = 0;

/*
 * Compute checksum.
 */
uint16_t net_checksum(void *data, size_t size)
{
	uint16_t *chunk, ret;
	uint32_t chksum;

	for (chksum = 0, chunk = (uint16_t *) data; size > 1; size -= 2)
		chksum += *chunk++;

	if (size == 1)
		chksum += *((uint8_t *) chunk);

	chksum = (chksum & 0xFFFF) + (chksum >> 16);
	chksum += (chksum >> 16);
	ret = ~chksum;

	return ret;
}

/*
 * Handle a socket buffer.
 */
void skb_handle(struct sk_buff *skb)
{
	switch(htons(skb->hh.eth_header->type)) {
		case ETHERNET_TYPE_ARP:
			arp_receive(skb);
			break;
		case ETHERNET_TYPE_IP:
			ip_receive(skb);
			break;
		default:
			break;
	}
}

/*
 * Register a network device.
 */
struct net_device *register_net_device(uint32_t io_base, uint16_t type, uint16_t family, const char *name)
{
	struct net_device *dev;

	/* network devices table full */
	if (nr_net_devices >= NR_NET_DEVICES || !name)
		return NULL;

	/* set net device */
	dev = &net_devices[nr_net_devices];
	dev->type = type;
	dev->family = family;
	dev->index = nr_net_devices + 1;
	dev->io_base = io_base;
	dev->flags = 0;
	dev->mtu = 1500;
	dev->tx_queue_len = 100;
	memset(&dev->stats, 0, sizeof(struct net_device_stats));

	/* set name */
	dev->name = strdup(name);
	if (!dev->name)
		return NULL;

	/* update number of net devices */
	nr_net_devices++;

	return dev;
}

/*
 * Find a network device.
 */
struct net_device *net_device_find(const char *name)
{
	int i;

	if (!name)
		return NULL;

	for (i = 0; i < nr_net_devices; i++)
		if (strcmp(net_devices[i].name, name) == 0)
			return &net_devices[i];

	return NULL;
}

/*
 * Transmit a network packet.
 */
void net_transmit(struct net_device *dev, struct sk_buff *skb)
{
	if (!skb)
		return;

	/* send and free packet */
	dev->start_xmit(skb, dev);
	skb_free(skb);
}
