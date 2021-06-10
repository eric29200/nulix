#include <net/ip.h>
#include <net/net.h>
#include <string.h>

/*
 * Build an IPv4 header.
 */
void ip_build_header(struct ip_header_t *ip_header, uint16_t length, uint16_t id,
                     uint8_t protocol, uint8_t *src_addr, uint8_t *dst_addr)
{
  ip_header->ihl = 5;
  ip_header->version = 4;
  ip_header->tos = 0;
  ip_header->length = htons(length);
  ip_header->id = htonl(id);
  ip_header->fragment_offset = 0;
  ip_header->flags = 0;
  ip_header->ttl = IPV4_TTL;
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
