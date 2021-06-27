#ifndef _TCP_H_
#define _TCP_H_

#include <stddef.h>
#include <net/sk_buff.h>

#define TCPCB_FLAG_FIN      0x01
#define TCPCB_FLAG_SYN      0x02
#define TCPCB_FLAG_RST      0x04
#define TCPCB_FLAG_PSH      0x08
#define TCPCB_FLAG_ACK      0x10
#define TCPCB_FLAG_URG      0x20
#define TCPCB_FLAG_ECE      0x40
#define TCPCB_FLAG_CWR      0x80

/*
 * TCP header.
 */
struct tcp_header_t {
  uint16_t  src_port;
  uint16_t  dst_port;
  uint32_t  seq;
  uint32_t  ack_seq;
  uint16_t  res1:4;
  uint16_t  doff:4;
  uint16_t  fin:1;
  uint16_t  syn:1;
  uint16_t  rst:1;
  uint16_t  psh:1;
  uint16_t  ack:1;
  uint16_t  urg:1;
  uint16_t  res2:2;
  uint16_t  window;
  uint16_t  chksum;
  uint16_t  urg_ptr;
};

/*
 * TCP check header.
 */
struct tcp_check_header_t {
  uint32_t  src_address;
  uint32_t  dst_address;
  uint8_t   zero;
  uint8_t   protocol;
  uint16_t  len;
};

void tcp_receive(struct sk_buff_t *skb);

#endif
