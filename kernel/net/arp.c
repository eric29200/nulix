#include <net/arp.h>
#include <net/ethernet.h>
#include <mm/mm.h>
#include <string.h>
#include <stderr.h>

/* ARP table (relation between MAC/IP addresses) */
static struct arp_table_entry_t arp_table[ARP_TABLE_SIZE];
static int arp_table_idx = 0;

/* broadcast MAC address */
static uint8_t broadcast_mac_addr[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/*
 * Format an ARP packet.
 */
static inline void arp_make_packet(struct net_device_t *net_dev, struct arp_packet_t *arp_packet,
                                   uint16_t op_code, uint8_t *dst_hardware_addr, uint8_t *dst_protocol_addr)
{
  arp_packet->hardware_type = htons(HARWARE_TYPE_ETHERNET);
  arp_packet->protocol = htons(ETHERNET_TYPE_IP);
  arp_packet->hardware_addr_len = 6;
  arp_packet->protocol_addr_len = 4;
  arp_packet->opcode = htons(op_code);
  memcpy(arp_packet->src_hardware_addr, net_dev->mac_addr, 6);
  memcpy(arp_packet->src_protocol_addr, net_dev->ip_addr, 4);
  memcpy(arp_packet->dst_hardware_addr, dst_hardware_addr, 6);
  memcpy(arp_packet->dst_protocol_addr, dst_protocol_addr, 4);
}

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

  /* make ARP packet */
  arp_make_packet(net_dev, arp_packet, ARP_REQUEST, dst_hardware_addr, dst_protocol_addr);

  /* send packet through ethernet */
  err = ethernet_send_packet(net_dev, broadcast_mac_addr, arp_packet, sizeof(struct arp_packet_t), ETHERNET_TYPE_ARP);

  /* free packet */
  kfree(arp_packet);

  return err;
}

/*
 * Receive an ARP packet.
 */
void arp_receive_packet(struct net_device_t *net_dev, void *packet, size_t packet_len)
{
  struct arp_packet_t *arp_packet;
  uint8_t dst_hardware_addr[6];
  uint8_t dst_protocol_addr[4];
  int i;

  /* check packet size */
  arp_packet = (struct arp_packet_t *) packet;
  if (packet_len < sizeof(struct arp_packet_t))
    return;

  /* update MAC/IP addresses relation */
  for (i = 0; i < arp_table_idx; i++)
    if (memcmp(arp_table[i].mac_addr, arp_packet->dst_hardware_addr, 6) == 0)
      memcpy(arp_table[i].ip_addr, arp_packet->dst_protocol_addr, 4);

  /* add MAC/IP adresses relation */
  if (i >= arp_table_idx) {
    memcpy(arp_table[arp_table_idx].mac_addr, arp_packet->dst_hardware_addr, 6);
    memcpy(arp_table[arp_table_idx].ip_addr, arp_packet->dst_protocol_addr, 4);

    /* update ARP table index */
    if (++arp_table_idx >= ARP_TABLE_SIZE)
      arp_table_idx = 0;
  }

  /* reply to arp request */
  if (ntohs(arp_packet->opcode) == ARP_REQUEST
      && net_dev && memcmp(net_dev->ip_addr, arp_packet->dst_protocol_addr, 4) == 0) {
      /* send to source */
      memcpy(dst_hardware_addr, arp_packet->src_hardware_addr, 6);
      memcpy(dst_protocol_addr, arp_packet->src_protocol_addr, 4);

      /* make ARP packet */
      arp_make_packet(net_dev, arp_packet, ARP_REPLY, dst_hardware_addr, dst_protocol_addr);

      /* send ARP reply */
      ethernet_send_packet(net_dev, broadcast_mac_addr, arp_packet, sizeof(struct arp_packet_t), ETHERNET_TYPE_ARP);
  }
}
