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
	uint8_t				hw_addr[MAX_ADDR_LEN];
	uint32_t			ip_addr;
	uint8_t				resolved;
	struct list_head		list;
	struct sk_buff_head 		skb_queue;
};

/* ARP table (relation between MAC/IP addresses) */
static LIST_HEAD(arp_entries);

/* broadcast MAC address */
static uint8_t broadcast_hw_addr[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static uint8_t zero_hw_addr[] = { 0, 0, 0, 0, 0, 0 };

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
 * Send packets waiting in an ARP entry.
 */
static void arp_send_queue(struct arp_table *entry)
{
	struct net_device *dev = entry->dev;
	struct sk_buff *skb;
	int ret;

	while ((skb = skb_dequeue(&entry->skb_queue)) != NULL) {
		/* rebuild hard header */
		ret = dev->rebuild_header(dev, entry->ip_addr, skb);
		if (ret) {
			skb_free(skb);
			continue;
		}

		/* transmit packet */
		net_transmit(dev, skb);
	}
}

/*
 * Update MAC/IP address relation.
 */
static int arp_update(struct net_device *dev, uint8_t *hw_addr, uint32_t ip_addr)
{
	struct arp_table *entry;

	/* update MAC/IP addresses relation */
	entry = arp_lookup(dev, ip_addr);
	if (entry) {
		memcpy(entry->hw_addr, hw_addr, dev->addr_len);
		entry->resolved = 1;

		/* retransmit packets waiting for mac address */
		arp_send_queue(entry);

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
	memcpy(entry->hw_addr, hw_addr, dev->addr_len);
	list_add_tail(&entry->list, &arp_entries);

	return 0;
}

/*
 * Send an ARP request.
 */
static int arp_send(struct net_device *dev, int type, uint8_t *dest_hw, uint32_t dest_ip)
{
	struct arp_header *arph;
	struct sk_buff *skb;
	uint8_t *buf;

	/* else send an ARP request */
	skb = skb_alloc(dev->hard_header_len + sizeof(struct arp_header) + 2 * (dev->addr_len + 4));
	if (!skb)
		return -ENOMEM;

	/* build ethernet header */
	dev->hard_header(skb, ETHERNET_TYPE_ARP, dev->hw_addr, broadcast_hw_addr);

	/* build ARP header */
	skb->nh.arp_header = arph = (struct arp_header *) skb_put(skb, sizeof(struct arp_header) + 2 * (dev->addr_len + 4));
	arph->hw_type = htons(HARDWARE_TYPE_ETHERNET);
	arph->protocol = htons(ETHERNET_TYPE_IP);
	arph->hw_addr_len = dev->addr_len;
	arph->prot_addr_len = 4;
	arph->opcode = htons(type);

	/* copy source hardware address */
	buf = (uint8_t *) arph + sizeof(struct arp_header);
	memcpy(buf, dev->hw_addr, dev->addr_len);
	buf += dev->addr_len;

	/* copy source IP address */
	memcpy(buf, &dev->ip_addr, 4);
	buf += 4;

	/* copy destination hardware address */
	memcpy(buf, dest_hw, dev->addr_len);
	buf += dev->addr_len;

	/* copy destination IP address */
	memcpy(buf, &dest_ip, 4);

	/* transmit request */
	net_transmit(dev, skb);

	return 0;
}

/*
 * Receive/decode an ARP packet.
 */
void arp_receive(struct sk_buff *skb)
{
	struct net_device *dev = skb->dev;
	uint32_t src_ip, dest_ip;
	struct arp_header *arph;
	uint8_t *src_hw;
	uint8_t *buf;

	/* decode ARP header */
	skb->nh.arp_header = arph = (struct arp_header *) skb->data;
	skb_pull(skb, sizeof(struct arp_header) + 2 * (dev->addr_len + 4));

	/* decode source hardware address */
	buf = (uint8_t *) arph + sizeof(struct arp_header);
	src_hw = buf;
	buf += dev->addr_len;

	/* decode source IP address */
	src_ip = *((uint32_t *) buf);
	buf += 4;

	/* skip hardware address */
	buf += dev->addr_len;

	/* decode destination IP address */
	dest_ip = *((uint32_t *) buf);

	/* reply to ARP request or add arp table entry */
	if (ntohs(arph->opcode) == ARP_REQUEST && dev->ip_addr != dest_ip)
	 	arp_send(dev, ARP_REPLY, src_hw, src_ip);
	else if (ntohs(arph->opcode) == ARP_REPLY)
		arp_update(dev, src_hw, src_ip);
}

/*
 * Resolve an IP address.
 */
int arp_find(struct net_device *dev, uint8_t *hw_addr, uint32_t ip_addr, struct sk_buff *skb)
{
	struct arp_table *entry;
	int ret;

	/* try to find address in cache */
	entry = arp_lookup(dev, ip_addr);
	if (entry && entry->resolved) {
		memcpy(hw_addr, entry->hw_addr, dev->addr_len);
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
	ret = arp_send(dev, ARP_REQUEST, zero_hw_addr, ip_addr);
	if (ret)
		return ret;

	/* queue socket buffer */
	skb_queue_tail(&entry->skb_queue, skb);

	return 1;
}