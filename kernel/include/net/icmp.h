#ifndef _ICMP_H_
#define _ICMP_H_

#include <net/net.h>

#define ICMP_TYPE_REPLY       0x00
#define ICMP_CODE_REPLY       0x00
#define ICMP_TYPE_REQUEST     0x08
#define ICMP_CODE_REQUEST     0x00

/*
 * ICMP packet.
 */
struct icmp_packet_t {
  uint8_t   type;
  uint8_t   code;
  uint16_t  chksum;
  uint16_t  id;
  uint16_t  sequence;
};

void icmp_receive_packet(struct net_device_t *net_dev, void *packet, size_t packet_len);

#endif
