#ifndef _ARP_H_
#define _ARP_H_

#include <net/sk_buff.h>

#define ARP_REQUEST         1
#define ARP_REPLY           2

#define ARP_TABLE_SIZE      512

/*
 * ARP header.
 */
struct arp_header_t {
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

/*
 * ARP table entry.
 */
struct arp_table_entry_t {
  uint8_t   mac_addr[6];
  uint8_t   ip_addr[4];
};

void arp_receive(struct sk_buff_t *skb);
struct arp_table_entry_t *arp_lookup(struct net_device_t *dev, uint8_t *ip_addr);
void arp_add_table(struct arp_header_t *arp_header);
void arp_reply_request(struct sk_buff_t *skb);
void arp_build_header(struct arp_header_t *arp_header, uint16_t op_code,
                      uint8_t *src_hardware_addr, uint8_t *src_protocol_addr,
                      uint8_t *dst_hardware_addr, uint8_t *dst_protocol_addr);

#endif
