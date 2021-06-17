#include <net/udp.h>
#include <net/net.h>
#include <net/ip.h>
#include <net/ethernet.h>
#include <net/arp.h>
#include <net/socket.h>
#include <proc/sched.h>
#include <string.h>
#include <stderr.h>

/*
 * Build an UDP header.
 */
static void udp_build_header(struct udp_header_t *udp_header, uint16_t src_port, uint16_t dst_port, uint16_t len)
{
  udp_header->src_port = htons(src_port);
  udp_header->dst_port = htons(dst_port);
  udp_header->len = htons(len);
  udp_header->chksum = 0;
}

/*
 * Receive/decode an UDP packet.
 */
void udp_receive(struct sk_buff_t *skb)
{
  skb->h.udp_header = (struct udp_header_t *) skb->data;
  skb_pull(skb, sizeof(struct udp_header_t));
}

/*
 * Send an UDP message.
 */
int udp_sendmsg(struct socket_t *sock, const struct msghdr_t *msg, int flags)
{
  struct arp_table_entry_t *arp_entry;
  struct sockaddr_in *dest_addr_in;
  uint8_t dest_ip[4], route_ip[4];
  struct sk_buff_t *skb;
  size_t len, i;
  void *buf;

  /* unused flags */
  UNUSED(flags);

  /* get destination IP */
  dest_addr_in = (struct sockaddr_in *) msg->msg_name;
  inet_ntoi(dest_addr_in->sin_addr, dest_ip);

  /* get route IP */
  ip_route(sock->dev, dest_ip, route_ip);

  /* find destination MAC address from arp */
  arp_entry = arp_lookup(sock->dev, route_ip);
  if (!arp_entry)
    return -EINVAL;

  /* compute data length */
  len = 0;
  for (i = 0; i < msg->msg_iovlen; i++)
    len += msg->msg_iov[i].iov_len;

  /* allocate a socket buffer */
  skb = skb_alloc(sizeof(struct ethernet_header_t) + sizeof(struct ip_header_t) + sizeof(struct udp_header_t) + len);
  if (!skb)
    return -ENOMEM;

  /* build ethernet header */
  skb->eth_header = (struct ethernet_header_t *) skb_put(skb, sizeof(struct ethernet_header_t));
  ethernet_build_header(skb->eth_header, sock->dev->mac_addr, arp_entry->mac_addr, ETHERNET_TYPE_IP);

  /* build ip header */
  skb->nh.ip_header = (struct ip_header_t *) skb_put(skb, sizeof(struct ip_header_t));
  ip_build_header(skb->nh.ip_header, 0, sizeof(struct ip_header_t) + sizeof(struct udp_header_t) + len, 0,
                  IPV4_DEFAULT_TTL, IP_PROTO_UDP, sock->dev->ip_addr, dest_ip);

  /* build udp header */
  skb->h.udp_header = (struct udp_header_t *) skb_put(skb, sizeof(struct udp_header_t));
  udp_build_header(skb->h.udp_header, ntohs(sock->sin.sin_port), ntohs(dest_addr_in->sin_port),
                   sizeof(struct udp_header_t) + len);

  /* copy message */
  buf = (struct udp_header_t *) skb_put(skb, len);
  for (i = 0; i < msg->msg_iovlen; i++) {
    memcpy(buf, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
    buf += msg->msg_iov[i].iov_len;
  }

  /* send message */
  sock->dev->send_packet(skb);

  /* free message */
  skb_free(skb);

  return len;
}

/*
 * Receive an UDP message.
 */
int udp_recvmsg(struct socket_t *sock, struct msghdr_t *msg, int flags)
{
  struct sk_buff_t *skb;
  size_t len, n, count = 0;
  size_t i;

  /* unused flags */
  UNUSED(flags);

  /* sleep until we receive a packet */
  for (;;) {
    /* signal received : restart system call */
    if (!sigisemptyset(&current_task->sigpend))
      return -ERESTARTSYS;

    /* message received : break */
    if (!list_empty(&sock->skb_list))
      break;

    /* sleep */
    task_sleep(&sock->waiting_chan);
  }

  /* get first message */
  skb = list_first_entry(&sock->skb_list, struct sk_buff_t, list);

  /* get UDP header */
  skb->h.udp_header = (struct udp_header_t *) (skb->head + sizeof(struct ethernet_header_t) + sizeof(struct udp_header_t));

  /* compute message length */
  len = (void *) skb->end - (void *) skb->h.udp_header;

  /* copy message */
  for (i = 0; i < msg->msg_iovlen; i++) {
    n = len > msg->msg_iov[i].iov_len ? msg->msg_iov[i].iov_len : len;
    memcpy(msg->msg_iov[i].iov_base, skb->h.udp_header, n);
    count += n;
  }

  /* remove and free socket buffer */
  list_del(&skb->list);
  skb_free(skb);

  return count;
}

/*
 * UDP protocol operations.
 */
struct prot_ops udp_prot_ops = {
  .recvmsg      = udp_recvmsg,
  .sendmsg      = udp_sendmsg,
};
