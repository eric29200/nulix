#ifndef _TCP_H_
#define _TCP_H_

#include <net/sk_buff.h>
#include <net/inet/ip.h>
#include <net/inet/net.h>
#include <stddef.h>

#define TCP_ESTABLISHED		1
#define TCP_CLOSE		7
#define TCP_LISTEN		10

#define TCPCB_FLAG_FIN		0x01
#define TCPCB_FLAG_SYN		0x02
#define TCPCB_FLAG_RST		0x04
#define TCPCB_FLAG_PSH		0x08
#define TCPCB_FLAG_ACK		0x10
#define TCPCB_FLAG_URG		0x20
#define TCPCB_FLAG_ECE		0x40
#define TCPCB_FLAG_CWR		0x80

#endif
