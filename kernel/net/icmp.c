#include <net/icmp.h>
#include <net/ethernet.h>
#include <net/net.h>
#include <net/arp.h>
#include <net/ip.h>
#include <net/socket.h>
#include <proc/sched.h>
#include <ipc/signal.h>
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
 * Handle an ICMP packet.
 */
int icmp_handle(struct socket_t *sock, struct sk_buff_t *skb)
{
  struct sk_buff_t *skb_new;

  /* check protocol */
  if (sock->protocol != skb->nh.ip_header->protocol)
    return -EINVAL;

  /* clone socket buffer */
  skb_new = skb_clone(skb);
  if (!skb_new)
    return -ENOMEM;

  /* push skb in socket queue */
  list_add_tail(&skb_new->list, &sock->skb_list);

  return 0;
}

/*
 * Send an ICMP message.
 */
int icmp_sendmsg(struct socket_t *sock, const struct msghdr_t *msg, int flags)
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
  skb = skb_alloc(sizeof(struct ethernet_header_t) + sizeof(struct ip_header_t) + len);
  if (!skb)
    return -ENOMEM;

  /* build ethernet header */
  skb->eth_header = (struct ethernet_header_t *) skb_put(skb, sizeof(struct ethernet_header_t));
  ethernet_build_header(skb->eth_header, sock->dev->mac_addr, arp_entry->mac_addr, ETHERNET_TYPE_IP);

  /* build ip header */
  skb->nh.ip_header = (struct ip_header_t *) skb_put(skb, sizeof(struct ip_header_t));
  ip_build_header(skb->nh.ip_header, 0, sizeof(struct ip_header_t) + len, 0,
                  IPV4_DEFAULT_TTL, IP_PROTO_ICMP, sock->dev->ip_addr, dest_ip);

  /* copy icmp message */
  skb->h.icmp_header = buf = (struct icmp_header_t *) skb_put(skb, len);
  for (i = 0; i < msg->msg_iovlen; i++) {
    memcpy(buf, msg->msg_iov[i].iov_base, msg->msg_iov[i].iov_len);
    buf += msg->msg_iov[i].iov_len;
  }

  /* recompute checksum */
  skb->h.icmp_header->chksum = 0;
  skb->h.icmp_header->chksum = net_checksum(skb->h.icmp_header, len);

  /* send message */
  sock->dev->send_packet(skb);

  /* free message */
  skb_free(skb);

  return len;
}

/*
 * Receive an ICMP message.
 */
int icmp_recvmsg(struct socket_t *sock, struct msghdr_t *msg, int flags)
{
  size_t len, n, count = 0;
  struct sockaddr_in *sin;
  struct sk_buff_t *skb;
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

  /* get ICMP header */
  skb->h.icmp_header = (struct icmp_header_t *) (skb->head + sizeof(struct ethernet_header_t) + sizeof(struct ip_header_t));

  /* compute message length */
  len = (void *) skb->end - (void *) skb->h.icmp_header;

  /* copy message */
  for (i = 0; i < msg->msg_iovlen; i++) {
    n = len > msg->msg_iov[i].iov_len ? msg->msg_iov[i].iov_len : len;
    memcpy(msg->msg_iov[i].iov_base, skb->h.icmp_header, n);
    count += n;
  }

  /* set source address */
  sin = (struct sockaddr_in *) msg->msg_name;
  sin->sin_family = AF_INET;
  sin->sin_port = 0;
  sin->sin_addr = inet_iton(skb->nh.ip_header->src_addr);

  /* remove and free socket buffer */
  list_del(&skb->list);
  skb_free(skb);

  return count;
}

/*
 * ICMP protocol operations.
 */
struct prot_ops icmp_prot_ops = {
  .handle       = icmp_handle,
  .recvmsg      = icmp_recvmsg,
  .sendmsg      = icmp_sendmsg,
};
