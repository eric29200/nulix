#ifndef _ARP_H_
#define _ARP_H_

#include <net/net.h>

#define ARP_REQUEST         1
#define ARP_REPLY           2

/*
 * ARP packet.
 */
struct arp_packet_t {
  uint16_t  hardware_type;
  uint16_t  protocol;
  uint8_t   hardware_addr_len;
  uint8_t   protocol_addr_len;
  uint16_t  opcode;
  uint8_t   src_hardware_addr[6];
  uint8_t   src_protocol_addr[4];
  uint8_t   dst_hardware_addr[6];
  uint8_t   dst_protocol_addr[4];
};

int arp_send_packet(struct net_device_t *net_dev, uint8_t *dst_hardware_addr, uint8_t *dst_protocol_addr);
void arp_receive_packet(void *packet, size_t packet_len);

#endif
