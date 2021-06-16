#include <net/ip.h>
#include <net/net.h>
#include <net/socket.h>
#include <string.h>

/*
 * Build an IPv4 header.
 */
void ip_build_header(struct ip_header_t *ip_header, uint8_t tos, uint16_t length, uint16_t id,
                     uint8_t ttl, uint8_t protocol, uint8_t *src_addr, uint8_t *dst_addr)
{
  ip_header->ihl = 5;
  ip_header->version = 4;
  ip_header->tos = tos;
  ip_header->length = htons(length);
  ip_header->id = htons(id);
  ip_header->fragment_offset = 0;
  ip_header->ttl = ttl;
  ip_header->protocol = protocol;
  memcpy(ip_header->src_addr, src_addr, 4);
  memcpy(ip_header->dst_addr, dst_addr, 4);
  ip_header->chksum = net_checksum(ip_header, sizeof(struct ip_header_t));
}

/*
 * Receive/decode an IP packet.
 */
void ip_receive(struct sk_buff_t *skb)
{
  skb->nh.ip_header = (struct ip_header_t *) skb->data;
  skb_pull(skb, sizeof(struct ip_header_t));
}

/*
 * Get IP route.
 */
void ip_route(struct net_device_t *dev, const uint8_t *dest_ip, uint8_t *route_ip)
{
  int i, same_net;

  /* check if destination ip is on same network */
  for (i = 0, same_net = 0; i < 4; i++) {
    same_net = (dev->ip_addr[i] & dev->ip_netmask[i]) == (dest_ip[i] & dev->ip_netmask[i]);
    if (!same_net)
      break;
  }

  /* set route ip */
  if (same_net)
    memcpy(route_ip, dest_ip, 4);
  else
    memcpy(route_ip, dev->ip_route, 4);
}
