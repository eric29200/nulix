#ifndef _RTL_8139_H_
#define _RTL_8139_H_

#include <stddef.h>

#define RTL8139_VENDOR_ID       0x10EC
#define RTL8139_DEVICE_ID       0x8139

#define RTL8139_MAC_ADDRESS     0x0

#define ROK                     0x01
#define TOK                     0x04

#define RX_BUFFER_SIZE          (8192 + 16 + 1500)

/*
 * Reception header.
 */
struct rtl8139_rx_header_t {
  uint16_t status;
  uint16_t size;
};

int init_rtl8139(uint8_t *ip_addr);
struct net_device_t *rtl8139_get_net_device();

#endif
