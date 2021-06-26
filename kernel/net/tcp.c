#include <net/tcp.h>
#include <net/socket.h>
#include <net/ip.h>
#include <net/arp.h>
#include <net/ethernet.h>
#include <string.h>
#include <stderr.h>

/*
 * Build a TCP header.
 */
static void tcp_build_header(struct tcp_header_t *tcp_header, uint16_t src_port, uint16_t dst_port,
                             uint32_t seq, uint32_t ack_seq, uint16_t window, uint16_t flags)
{
  tcp_header->src_port = htons(src_port);
  tcp_header->dst_port = htons(dst_port);
  tcp_header->seq = htonl(seq);
  tcp_header->ack_seq = htonl(ack_seq);
  tcp_header->res1 = 0;
  tcp_header->doff = sizeof(struct tcp_header_t) / 4;
  tcp_header->fin = (flags & TCPCB_FLAG_FIN) != 0;
  tcp_header->syn = (flags & TCPCB_FLAG_SYN) != 0;
  tcp_header->rst = (flags & TCPCB_FLAG_RST) != 0;
  tcp_header->psh = (flags & TCPCB_FLAG_PSH) != 0;
  tcp_header->ack = (flags & TCPCB_FLAG_ACK) != 0;
  tcp_header->urg = (flags & TCPCB_FLAG_URG) != 0;
  tcp_header->res2 = 0;
  tcp_header->window = htons(window);
  tcp_header->chksum = 0;
  tcp_header->urg_ptr = 0;
}

/*
 * Create a TCP message.
 */
static struct sk_buff_t *tcp_create_skb(struct socket_t *sock, uint32_t seq, uint32_t ack,
                                        uint16_t flags, void *msg, size_t len)
{
  struct arp_table_entry_t *arp_entry;
  uint8_t dest_ip[4], route_ip[4];
  struct sk_buff_t *skb;
  void *buf;

  /* get destination IP */
  inet_ntoi(sock->dst_sin.sin_addr, dest_ip);

  /* get route IP */
  ip_route(sock->dev, dest_ip, route_ip);

  /* find destination MAC address from arp */
  arp_entry = arp_lookup(sock->dev, route_ip);
  if (!arp_entry)
    return NULL;

  /* allocate a socket buffer */
  skb = skb_alloc(sizeof(struct ethernet_header_t) + sizeof(struct ip_header_t) + sizeof(struct tcp_header_t) + len);
  if (!skb)
    return NULL;

  /* build ethernet header */
  skb->eth_header = (struct ethernet_header_t *) skb_put(skb, sizeof(struct ethernet_header_t));
  ethernet_build_header(skb->eth_header, sock->dev->mac_addr, arp_entry->mac_addr, ETHERNET_TYPE_IP);

  /* build ip header */
  skb->nh.ip_header = (struct ip_header_t *) skb_put(skb, sizeof(struct ip_header_t));
  ip_build_header(skb->nh.ip_header, 0, sizeof(struct ip_header_t) + sizeof(struct tcp_header_t) + len, 0,
                  IPV4_DEFAULT_TTL, IP_PROTO_TCP, sock->dev->ip_addr, dest_ip);

  /* build tcp header */
  skb->h.tcp_header = (struct tcp_header_t *) skb_put(skb, sizeof(struct tcp_header_t));
  tcp_build_header(skb->h.tcp_header, ntohs(sock->src_sin.sin_port), ntohs(sock->dst_sin.sin_port),
                   seq, ack, 0xFFFFU, flags);

  /* copy message */
  buf = skb_put(skb, len);
  memcpy(buf, msg, len);

  return skb;
}

/*
 * Create a TCP connection.
 */
int tcp_connect(struct socket_t *sock)
{
  struct sk_buff_t *skb;

  /* create SYN message */
  skb = tcp_create_skb(sock, 1002, 0, TCPCB_FLAG_SYN, NULL, 0);
  if (!skb)
    return -EINVAL;

  /* send SYN message */
  sock->dev->send_packet(skb);
  skb_free(skb);

  return 0;
}


/*
 * TCP protocol operations.
 */
struct prot_ops tcp_prot_ops = {
  .handle       = NULL,
  .recvmsg      = NULL,
  .sendmsg      = NULL,
  .connect      = tcp_connect,
};
