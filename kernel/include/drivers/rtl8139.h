#ifndef _RTL_8139_H_
#define _RTL_8139_H_

#define RTL8139_VENDOR_ID       0x10EC
#define RTL8139_DEVICE_ID       0x8139

#define RTL8139_MAC_ADDRESS     0x0

#define RX_BUFFER_SIZE          (8192 + 16 + 1500)

int init_rtl8139();

#endif
