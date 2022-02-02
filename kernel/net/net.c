#include <net/net.h>
#include <net/socket.h>
#include <net/ethernet.h>
#include <net/arp.h>
#include <net/ip.h>
#include <net/udp.h>
#include <net/tcp.h>
#include <net/icmp.h>
#include <proc/sched.h>
#include <lib/list.h>
#include <stderr.h>
#include <string.h>

/* network devices */
static struct net_device_t net_devices[NR_NET_DEVICES];
static int nb_net_devices = 0;

/* sockets (defined in socket.c) */
extern struct socket_t sockets[NR_SOCKETS];

/*
 * Compute checksum.
 */
uint16_t net_checksum(void *data, size_t size)
{
  uint16_t *chunk, ret;
  uint32_t chksum;

  for (chksum = 0, chunk = (uint16_t *) data; size > 1; size -= 2)
    chksum += *chunk++;

  if (size == 1)
    chksum += *((uint8_t *) chunk);

  chksum = (chksum & 0xFFFF) + (chksum >> 16);
  chksum += (chksum >> 16);
  ret = ~chksum;

  return ret;
}

/*
 * Deliver a packet to sockets.
 */
static void skb_deliver_to_sockets(struct sk_buff_t *skb)
{
  int i, ret;

  /* find matching sockets */
  for (i = 0; i < NR_SOCKETS; i++) {
    /* unused socket */
    if (sockets[i].state == SS_FREE)
      continue;

    /* handle packet */
    if (sockets[i].ops && sockets[i].ops->handle) {
      ret = sockets[i].ops->handle(&sockets[i], skb);

      /* wake up waiting processes */
      if (ret == 0)
        task_wakeup_all(&sockets[i].waiting_chan);
    }
  }
}

/*
 * Handle a socket buffer.
 */
void skb_handle(struct sk_buff_t *skb)
{
  /* decode ethernet header */
  ethernet_receive(skb);

  /* handle packet */
  switch(htons(skb->eth_header->type)) {
    case ETHERNET_TYPE_ARP:
      /* decode ARP header */
      arp_receive(skb);

      /* reply to ARP request or add arp table entry */
      if (ntohs(skb->nh.arp_header->opcode) == ARP_REQUEST)
        arp_reply_request(skb);
      else if (ntohs(skb->nh.arp_header->opcode) == ARP_REPLY)
        arp_add_table(skb->nh.arp_header);

      break;
    case ETHERNET_TYPE_IP:
      /* decode IP header */
      ip_receive(skb);

      /* handle IPv4 only */
      if (skb->nh.ip_header->version != 4)
        break;

      /* check if packet is adressed to us */
      if (memcmp(skb->dev->ip_addr, skb->nh.ip_header->dst_addr, 4) != 0)
        break;

      /* go to next layer */
      switch (skb->nh.ip_header->protocol) {
        case IP_PROTO_UDP:
          udp_receive(skb);
          break;
        case IP_PROTO_TCP:
          tcp_receive(skb);
          break;
        case IP_PROTO_ICMP:
          icmp_receive(skb);

          /* handle ICMP requests */
          if (skb->h.icmp_header->type == ICMP_TYPE_ECHO) {
            icmp_reply_echo(skb);
            return;
          }

          break;
        default:
          break;
      }

      /* deliver message to sockets */
      skb_deliver_to_sockets(skb);

      break;
    default:
      break;
  }
}

/*
 * Network handler thread.
 */
static void net_handler_thread(void *arg)
{
  struct net_device_t *net_dev = (struct net_device_t *) arg;
  struct list_head_t *pos, *n;
  struct sk_buff_t *skb;

  for (;;) {
    /* handle incoming packets */
    list_for_each_safe(pos, n, &net_dev->skb_list) {
      /* get packet */
      skb = list_entry(pos, struct sk_buff_t, list);
      list_del(&skb->list);

      /* handle packet */
      skb_handle(skb);

      /* free packet */
      skb_free(skb);
    }

    /* wait for incoming packets */
    task_sleep(current_task->waiting_chan);
  }
}

/*
 * Register a network device.
 */
struct net_device_t *register_net_device(uint32_t io_base)
{
  struct net_device_t *net_dev;

  /* network devices table full */
  if (nb_net_devices >= NR_NET_DEVICES)
    return NULL;

  /* set net device */
  net_dev = &net_devices[nb_net_devices++];
  net_dev->io_base = io_base;
  INIT_LIST_HEAD(&net_dev->skb_list);

  /* create kernel thread to handle receive packets */
  net_dev->thread = create_kernel_thread(net_handler_thread, net_dev);
  if (!net_dev->thread)
    return NULL;

  return net_dev;
}

/*
 * Handle network packet (put it in device queue).
 */
void net_handle(struct net_device_t *net_dev, struct sk_buff_t *skb)
{
  if (!skb)
    return;

  /* put socket buffer in net device list */
  list_add_tail(&skb->list, &net_dev->skb_list);

  /* wake up handler */
  task_wakeup_all(net_dev->thread->waiting_chan);
}
