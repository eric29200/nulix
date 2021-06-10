#include <net/net.h>
#include <net/ethernet.h>
#include <net/arp.h>
#include <net/ip.h>
#include <net/icmp.h>
#include <string.h>

/* network devices */
static struct net_device_t net_devices[NR_NET_DEVICES];
static int nb_net_devices = 0;

/*
 * Register a newtork device.
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

  return net_dev;
}

/*
 * Compute checksum.
 */
uint16_t net_checksum(void *data, size_t size)
{
  uint32_t chksum;
  uint16_t *chunk;

  for (chksum = 0, chunk = (uint16_t *) data; size > 1; size -= 2, chunk += 1)
    chksum += *chunk;

  if (size == 1)
    chksum += *((uint8_t *) chunk);

  while (chksum > CHKSUM_MASK)
    chksum = (chksum & CHKSUM_MASK) + (chksum >> 16);

  return ~chksum & CHKSUM_MASK;
}

/*
 * Handle a socket buffer.
 */
void skb_handle(struct sk_buff_t *skb)
{
  /* decode ethernet header */
  ethernet_receive(skb);

  switch(htons(skb->eth_header->type)) {
    case ETHERNET_TYPE_ARP:
      /* decode ARP header */
      arp_receive(skb);

      /* update ARP table */
      arp_add_table(skb->nh.arp_header);

      /* reply to ARP request */
      if (ntohs(skb->nh.arp_header->opcode) == ARP_REQUEST)
        arp_reply_request(skb);

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
        case IP_PROTO_ICMP:
          icmp_receive(skb);

          /* handle ICMP requests */
          if (skb->h.icmp_header->type == ICMP_TYPE_REQUEST && skb->h.icmp_header->code == ICMP_CODE_REQUEST)
            icmp_reply_request(skb);

          break;
        default:
          break;
      }

      break;
    default:
      break;
  }
}
