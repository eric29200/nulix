#ifndef _NET_H_
#define _NET_H_

#include <stddef.h>

#define NR_NET_DEVICES      4

#define htons(s)            ((((s) & 0xFF) << 8) | (((s) & 0xFF00) >> 8))
#define ntohs(s)            (htons((s)))

/*
 * Network device.
 */
struct net_device_t {
  uint32_t  io_base;
  uint8_t   irq;
  uint8_t   mac_addr[6];
  uint8_t   ip_addr[4];
  void      (*send_packet)(void *data, size_t len);
};

struct net_device_t *register_net_device(uint32_t io_base);

#endif
