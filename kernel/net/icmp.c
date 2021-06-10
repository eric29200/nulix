#include <net/icmp.h>
#include <net/ethernet.h>
#include <net/net.h>
#include <net/ip.h>
#include <mm/mm.h>
#include <string.h>

void icmp_build_header(struct icmp_header_t *icmp_header, uint8_t type, uint32_t gateway)
{
  icmp_header->type = type;
  icmp_header->code = 0;
  icmp_header->un.gateway = htonl(gateway);
  icmp_header->chksum = net_checksum(icmp_header, sizeof(struct icmp_header_t));
}

/*
 * Receive/decode an ICMP packet.
 */
void icmp_receive(struct sk_buff_t *skb)
{
  skb->h.icmp_header = (struct icmp_header_t *) skb->data;
  skb_pull(skb, sizeof(struct icmp_header_t));
}

/*
 * Reply to an ICMP request.
 */
void icmp_reply_request(struct sk_buff_t *skb)
{
  struct sk_buff_t *skb_reply;
  uint16_t data_len, ip_len;
  uint8_t *data;

  /* compute data length */
  ip_len = ntohs(skb->nh.ip_header->length);
  data_len = ip_len - sizeof(struct ip_header_t) - sizeof(struct icmp_header_t);

  /* allocate reply */
  skb_reply = (struct sk_buff_t *) skb_alloc(sizeof(struct ethernet_header_t)
                                             + sizeof(struct ip_header_t)
                                             + sizeof(struct icmp_header_t)
                                             + data_len);
  if (!skb_reply)
    return;

  /* build ethernet header */
  skb_reply->eth_header = (struct ethernet_header_t *) skb_put(skb_reply, sizeof(struct ethernet_header_t));
  ethernet_build_header(skb_reply->eth_header, skb->dev->mac_addr, skb->eth_header->src_mac_addr, ETHERNET_TYPE_IP);

  /* build ip header */
  skb_reply->nh.ip_header = (struct ip_header_t *) skb_put(skb_reply, sizeof(struct ip_header_t));
  ip_build_header(skb_reply->nh.ip_header, ip_len, ntohl(skb->nh.ip_header->id),
                     IP_PROTO_ICMP, skb->dev->ip_addr, skb->nh.ip_header->src_addr);

  /* build icmp header */
  skb_reply->h.icmp_header = (struct icmp_header_t *) skb_put(skb_reply, sizeof(struct icmp_header_t));
  icmp_build_header(skb_reply->h.icmp_header, ICMP_TYPE_REPLY, ntohl(skb->h.icmp_header->un.gateway));

  /* push data */
  if (data_len > 0) {
    data = skb_put(skb_reply, data_len);
    memcpy(data, skb->tail, data_len);
  }

  /* send data */
  skb->dev->send_packet(skb_reply);

  /* free reply buffer */
  skb_free(skb_reply);
}
