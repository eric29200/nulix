#include <net/inet/arp.h>
#include <net/inet/net.h>
#include <net/inet/ethernet.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>

#define ARP_RES_TIME			(5 * HZ)
#define ARP_MAX_TRIES			3

/*
 * ARP table entry.
 */
struct arp_table {
	struct net_device *		dev;
	uint8_t				hw_addr[MAX_ADDR_LEN];
	uint32_t			ip_addr;
	uint32_t			flags;
	time_t				last_used;
	time_t				last_updated;
	int				retries;
	struct timer_event		timer;
	struct list_head		list;
	struct sk_buff_head 		skb_queue;
};

/* ARP table (relation between MAC/IP addresses) */
static LIST_HEAD(arp_entries);

/* broadcast MAC address */
static uint8_t broadcast_hw_addr[] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
static uint8_t zero_hw_addr[] = { 0, 0, 0, 0, 0, 0 };

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
	skb->arp = 1;
	skb->free = 1;
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
	dev_queue_xmit(dev, skb);

	return 0;
}

/*
 * Purge send queue.
 */
static void arp_purge_send_queue(struct arp_table *entry)
{
	struct sk_buff *skb;

	while ((skb = skb_dequeue(&entry->skb_queue)) != NULL)
		skb_free(skb);
}

/*
 * Free an ARP entry.
 */
static void arp_free_entry(struct arp_table *entry)
{
	arp_purge_send_queue(entry);
	del_timer(&entry->timer);
	list_del(&entry->list);
	kfree(entry);
}

/*
 * Handle a name resolution expiration.
 */
static void arp_expire_request(void *arg)
{
	struct arp_table *entry = (struct arp_table *) arg;
	struct net_device *dev = entry->dev;

	/* delete timer */
	del_timer(&entry->timer);

	/* retry */
	if (entry->last_updated && --entry->retries > 0) {
		entry->timer.expires = jiffies + ARP_RES_TIME;
		add_timer(&entry->timer);
		arp_send(dev, ARP_REQUEST, zero_hw_addr, entry->ip_addr);
		return;
	}

	/* host is really dead */
	arp_free_entry(entry);
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
	entry->last_updated = entry->last_used = jiffies;
	skb_queue_head_init(&entry->skb_queue);
	init_timer(&entry->timer, arp_expire_request, entry, 0);

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

	/* check if entry is up to date */
	if (!(entry->flags & ATF_COM))
		return;

	/* send waiting packets */
	while ((skb = skb_dequeue(&entry->skb_queue)) != NULL)
		dev_queue_xmit(dev, skb);
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
		del_timer(&entry->timer);

		/* update entry if it's not a permanent entry */
		if (!(entry->flags & ATF_PERM)) {
			entry->last_updated = jiffies;
			memcpy(entry->hw_addr, hw_addr, dev->addr_len);
		}

		/* new entry : mark it resolved and send waiting packets */
		if (!(entry->flags & ATF_COM)) {
			entry->flags |= ATF_COM;
			arp_send_queue(entry);
		}

		return 1;
	}

	/* allocate a new entry */
	entry = arp_alloc_entry();
	if (!entry)
		return -ENOMEM;

	/* set entry */
	entry->dev = dev;
	entry->ip_addr = ip_addr;
	entry->flags = ATF_COM;
	entry->last_updated = entry->last_used = jiffies;
	memcpy(entry->hw_addr, hw_addr, dev->addr_len);
	list_add_tail(&entry->list, &arp_entries);

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
	if (ntohs(arph->opcode) == ARP_REQUEST && dev->ip_addr == dest_ip)
	 	arp_send(dev, ARP_REPLY, src_hw, src_ip);
	else if (ntohs(arph->opcode) == ARP_REPLY)
		arp_update(dev, src_hw, src_ip);
}

/*
 * Create a new ARP entry.
 */
static struct arp_table *arp_new_entry(struct net_device *dev, uint32_t ip_addr, struct sk_buff *skb)
{
	struct arp_table *entry;

	/* allocate a new entry */
	entry = arp_alloc_entry();
	if (!entry)
		return NULL;

	/* set entry */
	entry->dev = dev;
	entry->ip_addr = ip_addr;
	list_add_tail(&entry->list, &arp_entries);

	/* queue socket buffer */
	if (skb)
		skb_queue_tail(&entry->skb_queue, skb);

	/* send an ARP request */
	entry->timer.expires = jiffies + ARP_RES_TIME;
	entry->retries = ARP_MAX_TRIES;
	add_timer(&entry->timer);
	arp_send(dev, ARP_REQUEST, zero_hw_addr, ip_addr);

	return entry;
}

/*
 * Resolve an IP address.
 */
int arp_find(struct net_device *dev, uint8_t *hw_addr, uint32_t ip_addr, struct sk_buff *skb)
{
	struct arp_table *entry;

	/* try to find address in cache */
	entry = arp_lookup(dev, ip_addr);
	if (entry) {
		/* entry up to date */
		if (entry->flags & ATF_COM) {
			entry->last_used = jiffies;
			memcpy(hw_addr, entry->hw_addr, dev->addr_len);
			return 0;
		}

		/* else queue socket buffer, to send it later (or free skb if host is dead : last_updated = 0) */
		if (skb) {
			if (entry->last_updated)
				skb_queue_tail(&entry->skb_queue, skb);
			else
				skb_free(skb);
		}

		return 1;
	}

	/* create a new ARP entry */
	entry = arp_new_entry(dev, ip_addr, skb);
	if (!entry && skb)
		skb_free(skb);

	return 1;
}