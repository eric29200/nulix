#include <net/arp.h>
#include <net/ethernet.h>
#include <mm/mm.h>
#include <string.h>
#include <stderr.h>

/* broadcast MAC address */
static uint8_t broadcast_mac_addr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/*
 * Send an ARP packet.
 */
int arp_send_packet(struct net_device_t *net_dev, uint8_t *dst_hardware_addr, uint8_t *dst_protocol_addr)
{
  struct arp_packet_t *arp_packet;
  int err;

  /* null net device */
  if (!net_dev)
    return -EINVAL;

  /* allocate an arp packet */
  arp_packet = (struct arp_packet_t *) kmalloc(sizeof(struct arp_packet_t));
  if (!arp_packet)
    return -ENOMEM;

  /* set ARP packet */
  arp_packet->hardware_type = htons(HARWARE_TYPE_ETHERNET);
  arp_packet->protocol = htons(ETHERNET_TYPE_IP);
  arp_packet->hardware_addr_len = 6;
  arp_packet->protocol_addr_len = 4;
  arp_packet->opcode = htons(ARP_REQUEST);
  memcpy(arp_packet->src_hardware_addr, net_dev->mac_addr, 6);
  memset(arp_packet->src_protocol_addr, 0, 4);
  memcpy(arp_packet->dst_hardware_addr, dst_hardware_addr, 6);
  memcpy(arp_packet->dst_protocol_addr, dst_protocol_addr, 4);

  /* send packet through ethernet */
  err = ethernet_send_packet(net_dev, broadcast_mac_addr, arp_packet, sizeof(struct arp_packet_t), ETHERNET_TYPE_ARP);

  /* free packet */
  kfree(arp_packet);

  return err;
}

/*
 * Receive an ARP packet.
 */
void arp_receive_packet(void *packet, size_t packet_len)
{
  UNUSED(packet);
  UNUSED(packet_len);
}
