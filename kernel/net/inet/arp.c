#include <net/inet/arp.h>
#include <net/inet/net.h>
#include <net/inet/ethernet.h>
#include <proc/sched.h>
#include <stdio.h>
#include <string.h>

/* ARP table (relation between MAC/IP addresses) */
static struct arp_table_entry arp_table[ARP_TABLE_SIZE];
static int arp_table_idx = 0;

/* broadcast MAC address */
static uint8_t broadcast_mac_addr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
static uint8_t zero_mac_addr[] = {0, 0, 0, 0, 0, 0};

/*
 * Build an ARP header.
 */
void arp_build_header(struct arp_header *arp_header, uint16_t op_code,
		      uint8_t *src_hardware_addr, uint8_t *src_protocol_addr,
		      uint8_t *dst_hardware_addr, uint8_t *dst_protocol_addr)
{
	arp_header->hardware_type = htons(HARWARE_TYPE_ETHERNET);
	arp_header->protocol = htons(ETHERNET_TYPE_IP);
	arp_header->hardware_addr_len = 6;
	arp_header->protocol_addr_len = 4;
	arp_header->opcode = htons(op_code);
	memcpy(arp_header->src_hardware_addr, src_hardware_addr, 6);
	memcpy(arp_header->src_protocol_addr, src_protocol_addr, 4);
	memcpy(arp_header->dst_hardware_addr, dst_hardware_addr, 6);
	memcpy(arp_header->dst_protocol_addr, dst_protocol_addr, 4);
}

/*
 * Receive/decode an ARP packet.
 */
void arp_receive(struct sk_buff *skb)
{
	skb->nh.arp_header = (struct arp_header *) skb->data;
	skb_pull(skb, sizeof(struct arp_header));
}

/*
 * Lookup for an arp table entry.
 */
struct arp_table_entry *arp_lookup(struct net_device *dev, uint8_t *ip_addr, int block)
{
	struct sk_buff *skb;
	int i;

	for (;;) {
		/* try to find address in cache */
		for (i = 0; i < ARP_TABLE_SIZE; i++)
			if (memcmp(arp_table[i].ip_addr, ip_addr, 4) == 0)
				return &arp_table[i];

		/* else send an ARP request */
		skb = skb_alloc(sizeof(struct ethernet_header) + sizeof(struct arp_header));
		if (!skb)
			return NULL;

		/* build ethernet header */
		skb->eth_header = (struct ethernet_header *) skb_put(skb, sizeof(struct ethernet_header));
		ethernet_build_header(skb->eth_header, dev->mac_addr, broadcast_mac_addr, ETHERNET_TYPE_ARP);

		/* build ARP header */
		skb->nh.arp_header = (struct arp_header *) skb_put(skb, sizeof(struct arp_header));
		arp_build_header(skb->nh.arp_header, ARP_REQUEST, dev->mac_addr, dev->ip_addr, zero_mac_addr, ip_addr);

		/* transmit request */
		net_transmit(dev, skb);

		/* do not block */
		if (!block)
			break;

		/* wait for response */
		current_task->timeout = jiffies + ms_to_jiffies(ARP_REQUEST_WAIT_MS);
		current_task->state = TASK_SLEEPING;
		schedule();
		current_task->timeout = 0;
	}

	return NULL;
}

/*
 * Extract MAC/IP address relation from an ARP packet.
 */
void arp_add_table(struct arp_header *arp_header)
{
	int i;

	/* update MAC/IP addresses relation */
	for (i = 0; i < arp_table_idx; i++) {
		if (memcmp(arp_table[i].mac_addr, arp_header->src_hardware_addr, 6) == 0) {
			memcpy(arp_table[i].ip_addr, arp_header->src_protocol_addr, 4);
			return;
		}
	}

	/* add MAC/IP adresses relation */
	if (i >= arp_table_idx) {
		memcpy(arp_table[arp_table_idx].mac_addr, arp_header->src_hardware_addr, 6);
		memcpy(arp_table[arp_table_idx].ip_addr, arp_header->src_protocol_addr, 4);

		/* update ARP table index */
		if (++arp_table_idx >= ARP_TABLE_SIZE)
			arp_table_idx = 0;
	}
}

/*
 * Reply to an ARP request.
 */
void arp_reply_request(struct sk_buff *skb)
{
	struct sk_buff *skb_reply;

	/* check IP address asked is us */
	if (memcmp(skb->dev->ip_addr, skb->nh.arp_header->dst_protocol_addr, 4) != 0)
		return;

	/* allocate reply buffer */
	skb_reply = skb_alloc(sizeof(struct ethernet_header) + sizeof(struct arp_header));
	if (!skb_reply)
		return;

	/* build ethernet header */
	skb_reply->eth_header = (struct ethernet_header *) skb_put(skb_reply, sizeof(struct ethernet_header));
	ethernet_build_header(skb_reply->eth_header, skb->dev->mac_addr, broadcast_mac_addr, ETHERNET_TYPE_ARP);

	/* build arp header */
	skb_reply->nh.arp_header = (struct arp_header *) skb_put(skb_reply, sizeof(struct arp_header));
	arp_build_header(skb_reply->nh.arp_header, ARP_REPLY, skb->dev->mac_addr, skb->dev->ip_addr,
			 skb->nh.arp_header->src_hardware_addr, skb->nh.arp_header->src_protocol_addr);

	/* transmit reply */
	net_transmit(skb->dev, skb_reply);
}
