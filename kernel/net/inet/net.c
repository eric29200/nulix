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
	/* decode ethernet header */
	ethernet_receive(skb);

	/* handle packet */
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
struct net_device *register_net_device(uint32_t io_base, uint16_t type)
{
	struct net_device *net_dev;
	char tmp[32];
	size_t len;

	/* network devices table full */
	if (nr_net_devices >= NR_NET_DEVICES)
		return NULL;

	/* set net device */
	net_dev = &net_devices[nr_net_devices];
	net_dev->type = type;
	net_dev->index = nr_net_devices + 1;
	net_dev->io_base = io_base;
	net_dev->flags = 0;
	net_dev->mtu = 1500;
	net_dev->tx_queue_len = 100;
	memset(&net_dev->stats, 0, sizeof(struct net_device_stats));

	/* set name */
	len = sprintf(tmp, "eth%d", nr_net_devices);

	/* allocate name */
	net_dev->name = (char *) kmalloc(len + 1);
	if (!net_dev->name)
		return NULL;

	/* set name */
	memcpy(net_dev->name, tmp, len + 1);

	/* update number of net devices */
	nr_net_devices++;

	return net_dev;
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
 * Handle network packet.
 */
void net_handle(struct sk_buff *skb)
{
	if (!skb)
		return;

	/* handle packet */
	skb_handle(skb);
	skb_free(skb);
}

/*
 * Transmit a network packet.
 */
void net_transmit(struct net_device *net_dev, struct sk_buff *skb)
{
	if (!skb)
		return;

	/* send and free packet */
	net_dev->start_xmit(skb, net_dev);
	skb_free(skb);
}
