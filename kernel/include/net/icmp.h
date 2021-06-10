#ifndef _ICMP_H_
#define _ICMP_H_

#include <net/sk_buff.h>

#define ICMP_TYPE_REPLY       0x00
#define ICMP_CODE_REPLY       0x00
#define ICMP_TYPE_REQUEST     0x08
#define ICMP_CODE_REQUEST     0x00

/*
 * ICMP header.
 */
struct icmp_header_t {
  uint8_t   type;
  uint8_t   code;
  uint16_t  chksum;
  union {
    struct {
      uint16_t  id;
      uint16_t  sequence;
    } echo;
    uint32_t gateway;
  } un;
};

void icmp_receive(struct sk_buff_t *skb);
void icmp_reply_request(struct sk_buff_t *skb);

#endif
