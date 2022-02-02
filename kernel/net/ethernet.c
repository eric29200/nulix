#include <net/ethernet.h>
#include <net/net.h>
#include <net/arp.h>
#include <net/ip.h>
#include <string.h>
#include <stderr.h>

/*
 * Build an ethernet header.
 */
void ethernet_build_header(struct ethernet_header_t *eth_header, uint8_t *src_mac_addr, uint8_t *dst_mac_addr,
                           uint16_t type)
{
  memcpy(eth_header->src_mac_addr, src_mac_addr, 6);
  if (dst_mac_addr)
    memcpy(eth_header->dst_mac_addr, dst_mac_addr, 6);
  eth_header->type = htons(type);
}

/*
 * Rebuild an ethernet header (find destination MAC address).
 */
int ethernet_rebuild_header(struct net_device_t *dev, struct sk_buff_t *skb)
{
  struct arp_table_entry_t *arp_entry;
  uint8_t route_ip[4];

  /* ARP request : do not rebuild header */
  if (skb->eth_header->type == htons(ETHERNET_TYPE_ARP))
    return 0;

  /* get route IP */
  ip_route(dev, skb->nh.ip_header->dst_addr, route_ip);

  /* find ARP entry */
  arp_entry = arp_lookup(dev, route_ip, 0);
  if (!arp_entry)
    return -EINVAL;

  /* set MAC address */
  memcpy(skb->eth_header->dst_mac_addr, arp_entry->mac_addr, 6);

  return 0;
}

/*
 * Receive/decode an ethernet packet.
 */
void ethernet_receive(struct sk_buff_t *skb)
{
  skb->eth_header = (struct ethernet_header_t *) skb->data;
  skb_pull(skb, sizeof(struct ethernet_header_t));
}
