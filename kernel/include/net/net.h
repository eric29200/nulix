#ifndef _NET_H_
#define _NET_H_

#include <stddef.h>

#define NR_NET_DEVICES      4

/*
 * Network device.
 */
struct net_device_t {
  uint32_t  io_base;
  uint8_t   irq;
  uint8_t   mac_address[6];
};

struct net_device_t *register_net_device(uint32_t io_base);

#endif
