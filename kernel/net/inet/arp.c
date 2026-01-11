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
	struct net_device *		dev;
	uint8_t				ha_addr[MAX_ADDR_LEN];
	uint32_t			ip_addr;
	uint8_t				resolved;
	struct list_head		list;
	struct sk_buff_head 		skb_queue;
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
	arp_header->hw_type = htons(HARWARE_TYPE_ETHERNET);
	arp_header->protocol = htons(ETHERNET_TYPE_IP);
	arp_header->hw_addr_len = 6;
	arp_header->prot_addr_len = 4;
	arp_header->opcode = htons(op_code);
	memcpy(arp_header->src_hw_addr, src_hardware_addr, 6);
	arp_header->src_prot_addr = src_protocol_addr;
	memcpy(arp_header->dst_hw_addr, dst_hardware_addr, 6);
	arp_header->dst_prot_addr = dst_protocol_addr;
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
		memcpy(entry->ha_addr, mac_addr, dev->addr_len);
		entry->resolved = 1;

		/* retransmit packets waiting for mac address */
		while ((skb = skb_dequeue(&entry->skb_queue)) != NULL) {
			memcpy(skb->hh.eth_header->dst_mac_addr, entry->ha_addr, dev->addr_len);
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
	memcpy(entry->ha_addr, mac_addr, dev->addr_len);
	list_add_tail(&entry->list, &arp_entries);

	return 0;
}

/*
 * Send an ARP request.
 */
static int arp_send(struct net_device *dev, int type, uint8_t *dest_hw, uint32_t dest_ip)
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
	arp_build_header(skb->nh.arp_header, type, dev->mac_addr, dev->ip_addr, dest_hw, dest_ip);

	/* transmit request */
	net_transmit(dev, skb);

	return 0;
}

/*
 * Receive/decode an ARP packet.
 */
void arp_receive(struct sk_buff *skb)
{
	struct arp_header *arph;

	/* decode ARP header */
	skb->nh.arp_header = arph = (struct arp_header *) skb->data;
	skb_pull(skb, sizeof(struct arp_header));

	/* reply to ARP request or add arp table entry */
	if (ntohs(arph->opcode) == ARP_REQUEST && skb->dev->ip_addr != arph->dst_prot_addr)
	 	arp_send(skb->dev, ARP_REPLY, arph->src_hw_addr, arph->src_prot_addr);
	else if (ntohs(arph->opcode) == ARP_REPLY)
		arp_update(skb->dev, arph->src_prot_addr, arph->src_hw_addr);
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
		memcpy(mac_addr, entry->ha_addr, dev->addr_len);
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
	ret = arp_send(dev, ARP_REQUEST, zero_mac_addr, ip_addr);
	if (ret)
		return ret;

	/* queue socket buffer */
	skb_queue_tail(&entry->skb_queue, skb);

	return 1;
}