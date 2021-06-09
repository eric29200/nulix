#ifndef _ETHERNET_H_
#define _ETHERNET_H_

#include <net/net.h>

#define ETHERNET_TYPE_ARP       0x0806
#define ETHERNET_TYPE_IP        0x0800

#define HARWARE_TYPE_ETHERNET   0x01

/*
 * Ethernet frame.
 */
struct ethernet_frame_t {
  uint8_t   dst_mac_addr[6];
  uint8_t   src_mac_addr[6];
  uint16_t  type;
  uint8_t   data[];
};

int ethernet_send_packet(struct net_device_t *net_dev, uint8_t *dst_mac_addr,
                         void *data, size_t data_len, uint16_t protocol);
void ethernet_receive_packet(struct net_device_t *net_dev, void *packet, size_t packet_len);

#endif
