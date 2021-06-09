#include <net/ethernet.h>
#include <net/arp.h>
#include <mm/mm.h>
#include <string.h>
#include <stderr.h>

/*
 * Send a packet through ethernet protocol.
 */
int ethernet_send_packet(struct net_device_t *net_dev, uint8_t *dst_mac_addr,
                         void *data, size_t data_len, uint16_t protocol)
{
  struct ethernet_frame_t *eth_frame;

  /* send packet not implemented on net device */
  if (!net_dev || !net_dev->send_packet)
    return -EINVAL;

  /* allocate an ethernet frame */
  eth_frame = (struct ethernet_frame_t *) kmalloc(sizeof(struct ethernet_frame_t) + data_len);
  if (!eth_frame)
    return -ENOMEM;

  /* set ethernet frame */
  eth_frame->type = htons(protocol);
  memcpy(eth_frame->dst_mac_addr, dst_mac_addr, 6);
  memcpy(eth_frame->src_mac_addr, net_dev->mac_addr, 6);
  memcpy(eth_frame->data, data, data_len);

  /* send packet */
  net_dev->send_packet(eth_frame, sizeof(struct ethernet_frame_t) + data_len);

  /* free packet */
  kfree(eth_frame);

  return 0;
}

/*
 * Receive an ethernet packet.
 */
void ethernet_receive_packet(void *packet, size_t packet_len)
{
  struct ethernet_frame_t *eth_frame;
  int data_len;

  /* compute data size */
  data_len = packet_len - sizeof(struct ethernet_frame_t);
  if (data_len < 0)
    return;

  /* get ethernet frame */
  eth_frame = (struct ethernet_frame_t *) packet;

  /* handle ARP packet */
  if (ntohs(eth_frame->type) == ETHERNET_TYPE_ARP)
    arp_receive_packet(packet + sizeof(struct ethernet_frame_t), data_len);
}
