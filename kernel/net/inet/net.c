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
LIST_HEAD(net_devices);
static int nr_net_devices = 0;

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
			skb_free(skb);
			break;
	}
}

/*
 * Register a network device.
 */
struct net_device *register_net_device(uint32_t io_base, uint16_t type, uint16_t family, const char *name)
{
	struct net_device *dev;

	/* allocate a new net device */
	dev = (struct net_device *) kmalloc(sizeof(struct net_device));
	if (!dev)
		return NULL;

	/* set net device */
	memset(dev, 0, sizeof(struct net_device));
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
	if (!dev->name) {
		kfree(dev);
		return NULL;
	}

	/* add network device */
	list_add_tail(&dev->list, &net_devices);

	/* update number of netwrok devices */
	nr_net_devices++;

	return dev;
}

/*
 * Unregister a network device.
 */
void unregister_net_device(struct net_device *dev)
{
	list_del(&dev->list);
	kfree(dev);
}

/*
 * Find a network device.
 */
struct net_device *net_device_find(const char *name)
{
	struct net_device *dev;
	struct list_head *pos;

	if (!name)
		return NULL;

	list_for_each(pos, &net_devices) {
		dev = list_entry(pos, struct net_device, list);
		if (strcmp(dev->name, name) == 0)
			return dev;
	}

	return NULL;
}