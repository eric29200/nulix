#include <net/icmp.h>
#include <net/ethernet.h>
#include <net/net.h>
#include <net/ip.h>
#include <net/socket.h>
#include <mm/mm.h>
#include <string.h>
#include <stderr.h>

/*
 * Receive/decode an ICMP packet.
 */
void icmp_receive(struct sk_buff_t *skb)
{
  skb->h.icmp_header = (struct icmp_header_t *) skb->data;
  skb_pull(skb, sizeof(struct icmp_header_t));
}

/*
 * Reply to an ICMP echo request.
 */
void icmp_reply_echo(struct sk_buff_t *skb)
{
  struct sk_buff_t *skb_reply;
  uint16_t data_len, ip_len;

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
  ip_build_header(skb_reply->nh.ip_header, skb->nh.ip_header->tos, ip_len, ntohs(skb->nh.ip_header->id),
                  skb->nh.ip_header->ttl, IP_PROTO_ICMP, skb->dev->ip_addr, skb->nh.ip_header->src_addr);

  /* copy icmp header */
  skb_reply->h.icmp_header = (struct icmp_header_t *) skb_put(skb_reply, sizeof(struct icmp_header_t) + data_len);
  memcpy(skb_reply->h.icmp_header, skb->h.icmp_header, sizeof(struct icmp_header_t) + data_len);

  /* set reply */
  skb_reply->h.icmp_header->type = 0;
  skb_reply->h.icmp_header->code = 0;
  skb_reply->h.icmp_header->chksum = 0;
  skb_reply->h.icmp_header->chksum = net_checksum(skb_reply->h.icmp_header, sizeof(struct icmp_header_t) + data_len);

  /* send data */
  skb->dev->send_packet(skb_reply);

  /* free reply buffer */
  skb_free(skb_reply);
}

/*
 * ICMP send to.
 */
static int icmp_sendto(struct socket_t *sock, const void *buf, size_t len,
                       const struct sockaddr *dest_addr, size_t addrlen)
{
  return -EINVAL;
}

/*
 * ICMP protocol operations.
 */
struct prot_ops icmp_prot_ops = {
  .sendto       = icmp_sendto,
};
