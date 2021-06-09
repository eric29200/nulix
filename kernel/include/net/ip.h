#ifndef _IP_H_
#define _IP_H_

#include <net/net.h>

#define IP_PROTO_ICMP               0x01
#define IP_PROTO_TCP                0x06
#define IP_PROTO_UDP                0x11

#define ip_version(ip_packet)       (((ip_packet)->version & 0xF0) >> 4)

/*
 * IP packet.
 */
struct ip_packet_t {
  uint8_t   ihl:4;
  uint8_t   version:4;
  uint8_t   tos;
  uint16_t  length;
  uint16_t  id;
  uint16_t  fragment_offset:13;
  uint8_t   flags:3;
  uint8_t   ttl;
  uint8_t   protocol;
  uint16_t  chksum;
  uint8_t   src_addr[4];
  uint8_t   dst_addr[4];
  uint8_t   data[];
};

void ip_receive_packet(struct net_device_t *net_dev, void *packet, size_t packet_len);

#endif
