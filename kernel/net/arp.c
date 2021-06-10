#include <net/arp.h>
#include <net/net.h>
#include <net/ethernet.h>
#include <string.h>

/* ARP table (relation between MAC/IP addresses) */
static struct arp_table_entry_t arp_table[ARP_TABLE_SIZE];
static int arp_table_idx = 0;

/* broadcast MAC address */
static uint8_t broadcast_mac_addr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/*
 * Build an ARP header.
 */
void arp_build_header(struct arp_header_t *arp_header, uint16_t op_code,
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
void arp_receive(struct sk_buff_t *skb)
{
  skb->nh.arp_header = (struct arp_header_t *) skb->data;
  skb_pull(skb, sizeof(struct arp_header_t));
}

/*
 * Extract MAC/IP address relation from an ARP packet.
 */
void arp_add_table(struct arp_header_t *arp_header)
{
  int i;

  /* update MAC/IP addresses relation */
  for (i = 0; i < arp_table_idx; i++)
    if (memcmp(arp_table[i].mac_addr, arp_header->dst_hardware_addr, 6) == 0)
      memcpy(arp_table[i].ip_addr, arp_header->dst_protocol_addr, 4);

  /* add MAC/IP adresses relation */
  if (i >= arp_table_idx) {
    memcpy(arp_table[arp_table_idx].mac_addr, arp_header->dst_hardware_addr, 6);
    memcpy(arp_table[arp_table_idx].ip_addr, arp_header->dst_protocol_addr, 4);

    /* update ARP table index */
    if (++arp_table_idx >= ARP_TABLE_SIZE)
      arp_table_idx = 0;
  }
}

/*
 * Reply to an ARP request.
 */
void arp_reply_request(struct sk_buff_t *skb)
{
  struct sk_buff_t *skb_reply;

  /* check IP address asked is us */
  if (memcmp(skb->dev->ip_addr, skb->nh.arp_header->dst_protocol_addr, 4) != 0)
    return;

  /* allocate reply buffer */
  skb_reply = skb_alloc(sizeof(struct ethernet_header_t) + sizeof(struct arp_header_t));
  if (!skb_reply)
    return;

  /* build ethernet header */
  skb_reply->eth_header = (struct ethernet_header_t *) skb_put(skb_reply, sizeof(struct ethernet_header_t));
  ethernet_build_header(skb_reply->eth_header, skb->dev->mac_addr, broadcast_mac_addr, ETHERNET_TYPE_ARP);

  /* build arp header */
  skb_reply->nh.arp_header = (struct arp_header_t *) skb_put(skb_reply, sizeof(struct arp_header_t));
  arp_build_header(skb_reply->nh.arp_header, ARP_REPLY, skb->dev->mac_addr, skb->dev->ip_addr,
                   skb->nh.arp_header->src_hardware_addr, skb->nh.arp_header->src_protocol_addr);

  /* send reply buffer */
  skb->dev->send_packet(skb_reply);

  /* free reply buffer */
  skb_free(skb_reply);
}
