#include <net/inet/arp.h>
#include <net/inet/net.h>
#include <net/inet/ethernet.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>

/*
 * ARP table entry.
 */
struct arp_table {
	struct net_device *	dev;
	uint8_t			mac_addr[6];
	uint32_t		ip_addr;
	uint8_t			resolved;
	struct list_head	list;
	struct sk_buff_head 	skb_queue;
};

/* ARP table (relation between MAC/IP addresses) */
static LIST_HEAD(arp_entries);

/* broadcast MAC address */
static uint8_t broadcast_mac_addr[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static uint8_t zero_mac_addr[] = { 0, 0, 0, 0, 0, 0 };

/*
 * Build an ARP header.
 */
static void arp_build_header(struct arp_header *arp_header, uint16_t op_code,
	uint8_t *src_hardware_addr, uint32_t src_protocol_addr,
	uint8_t *dst_hardware_addr, uint32_t dst_protocol_addr)
{
	arp_header->hardware_type = htons(HARWARE_TYPE_ETHERNET);
	arp_header->protocol = htons(ETHERNET_TYPE_IP);
	arp_header->hardware_addr_len = 6;
	arp_header->protocol_addr_len = 4;
	arp_header->opcode = htons(op_code);
	memcpy(arp_header->src_hardware_addr, src_hardware_addr, 6);
	arp_header->src_protocol_addr = src_protocol_addr;
	memcpy(arp_header->dst_hardware_addr, dst_hardware_addr, 6);
	arp_header->dst_protocol_addr = dst_protocol_addr;
}

/*
 * Allocate a new ARP entry.
 */
static struct arp_table *arp_alloc_entry()
{
	struct arp_table *entry;

	/* allocate new entry */
	entry = (struct arp_table *) kmalloc(sizeof(struct arp_table));
	if (!entry)
		return NULL;

	/* init entry */
	memset(entry, 0, sizeof(struct arp_table));
	skb_queue_head_init(&entry->skb_queue);

	return entry;
}

/*
 * Look for an entry.
 */
static struct arp_table *arp_lookup(struct net_device *dev, uint32_t ip_addr)
{
	struct arp_table *entry;
	struct list_head *pos;

	list_for_each(pos, &arp_entries) {
		entry = list_entry(pos, struct arp_table, list);
		if (entry->ip_addr == ip_addr && (!dev || entry->dev == dev))
			return entry;
	}

	return NULL;
}

/*
 * Update MAC/IP address relation.
 */
static int arp_update(struct net_device *dev, uint32_t ip_addr, uint8_t *mac_addr)
{
	struct arp_table *entry;
	struct sk_buff *skb;

	/* update MAC/IP addresses relation */
	entry = arp_lookup(dev, ip_addr);
	if (entry) {
		memcpy(entry->mac_addr, mac_addr, 6);
		entry->resolved = 1;

		/* retransmit packets waiting for mac address */
		while ((skb = skb_dequeue(&entry->skb_queue)) != NULL) {
			memcpy(skb->hh.eth_header->dst_mac_addr, entry->mac_addr, 6);
			net_transmit(dev, skb);
		}

		return 0;
	}

	/* allocate a new entry */
	entry = arp_alloc_entry();
	if (!entry)
		return -ENOMEM;

	/* set entry */
	entry->dev = dev;
	entry->ip_addr = ip_addr;
	entry->resolved = 1;
	memcpy(entry->mac_addr, mac_addr, 6);
	list_add_tail(&entry->list, &arp_entries);

	return 0;
}

/*
 * Reply to an ARP request.
 */
static void arp_reply_request(struct sk_buff *skb)
{
	struct sk_buff *skb_reply;

	/* check IP address asked is us */
	if (skb->dev->ip_addr != skb->nh.arp_header->dst_protocol_addr)
		return;

	/* allocate reply buffer */
	skb_reply = skb_alloc(sizeof(struct ethernet_header) + sizeof(struct arp_header));
	if (!skb_reply)
		return;

	/* build ethernet header */
	skb_reply->hh.eth_header = (struct ethernet_header *) skb_put(skb_reply, sizeof(struct ethernet_header));
	ethernet_build_header(skb_reply->hh.eth_header, skb->dev->mac_addr, broadcast_mac_addr, ETHERNET_TYPE_ARP);

	/* build arp header */
	skb_reply->nh.arp_header = (struct arp_header *) skb_put(skb_reply, sizeof(struct arp_header));
	arp_build_header(skb_reply->nh.arp_header, ARP_REPLY, skb->dev->mac_addr, skb->dev->ip_addr,
			 skb->nh.arp_header->src_hardware_addr, skb->nh.arp_header->src_protocol_addr);

	/* transmit reply */
	net_transmit(skb->dev, skb_reply);
}

/*
 * Receive/decode an ARP packet.
 */
void arp_receive(struct sk_buff *skb)
{
	/* decode ARP header */
	skb->nh.arp_header = (struct arp_header *) skb->data;
	skb_pull(skb, sizeof(struct arp_header));

	/* reply to ARP request or add arp table entry */
	if (ntohs(skb->nh.arp_header->opcode) == ARP_REQUEST)
		arp_reply_request(skb);
	else if (ntohs(skb->nh.arp_header->opcode) == ARP_REPLY)
		arp_update(skb->dev, skb->nh.arp_header->src_protocol_addr, skb->nh.arp_header->src_hardware_addr);
}

/*
 * Send an ARP request.
 */
static int arp_send(struct net_device *dev, uint32_t ip_addr)
{
	struct sk_buff *skb;

	/* else send an ARP request */
	skb = skb_alloc(sizeof(struct ethernet_header) + sizeof(struct arp_header));
	if (!skb)
		return -ENOMEM;

	/* build ethernet header */
	skb->hh.eth_header = (struct ethernet_header *) skb_put(skb, sizeof(struct ethernet_header));
	ethernet_build_header(skb->hh.eth_header, dev->mac_addr, broadcast_mac_addr, ETHERNET_TYPE_ARP);

	/* build ARP header */
	skb->nh.arp_header = (struct arp_header *) skb_put(skb, sizeof(struct arp_header));
	arp_build_header(skb->nh.arp_header, ARP_REQUEST, dev->mac_addr, dev->ip_addr, zero_mac_addr, ip_addr);

	/* transmit request */
	net_transmit(dev, skb);

	return 0;
}

/*
 * Resolve an IP address.
 */
int arp_find(struct net_device *dev, uint32_t ip_addr, uint8_t *mac_addr, struct sk_buff *skb)
{
	struct arp_table *entry;
	int ret;

	/* try to find address in cache */
	entry = arp_lookup(dev, ip_addr);
	if (entry && entry->resolved) {
		memcpy(mac_addr, entry->mac_addr, 6);
		return 0;
	}

	/* ARP entry exists but address is not resolved : send a request */
	if (entry)
		goto request;

	/* allocate a new entry */
	entry = arp_alloc_entry();
	if (!entry)
		return -ENOMEM;

	/* set entry */
	entry->dev = dev;
	entry->ip_addr = ip_addr;
	list_add_tail(&entry->list, &arp_entries);

request:
	/* send an ARP request */
	ret = arp_send(dev, ip_addr);
	if (ret)
		return ret;

	/* queue socket buffer */
	skb_queue_tail(&entry->skb_queue, skb);

	return 1;
}