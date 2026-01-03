#ifndef _RTL_8139_H_
#define _RTL_8139_H_

#include <stddef.h>

#define RTL8139_VENDOR_ID		0x10EC
#define RTL8139_DEVICE_ID		0x8139

#define RTL8139_MAC_ADDRESS		0x0

#define RX_BUF_SIZE			(8192 + 16 + 1500)
#define TX_BUF_SIZE			1536

#define NUM_TX_DESC			4

/*
 * Realtek 8139 private data.
 */
struct rtl8139_private {
	size_t			cur_tx;
	uint8_t *		tx_bufs;
	uint8_t *		tx_buf[NUM_TX_DESC];
	uint8_t *		rx_buf;
};

/*
 * Reception header.
 */
struct rtl8139_rx_header {
	uint16_t status;
	uint16_t size;
};


int init_rtl8139(uint8_t *ip_addr, uint8_t *ip_netmask, uint8_t *ip_route);
struct net_device *rtl8139_get_net_device();

#endif
