#ifndef _SOCK_H_
#define _SOCK_H_

#include <net/inet/sk_buff.h>
#include <net/socket.h>

/*
 * Inet socket address.
 */
struct sockaddr_in {
	uint16_t		sin_family;
	uint16_t		sin_port;
	uint32_t		sin_addr;
	uint8_t		 	sin_pad[8];
};

/*
 * Inet socket structure.
 */
struct sock_t {
	struct net_device_t *	dev;
	uint16_t		protocol;
	uint16_t		type;
	struct sockaddr_in	src_sin;
	struct sockaddr_in	dst_sin;
	uint32_t		seq_no;
	uint32_t		ack_no;
	off_t			msg_position;
	struct socket_t *	sock;
	struct proto_t *	prot;
	struct list_head_t	skb_list;
};

/*
 * Inet protocol.
 */
struct proto_t {
	int (*handle)(struct sock_t *, struct sk_buff_t *);
	int (*close)(struct sock_t *);
	int (*recvmsg)(struct sock_t *, struct msghdr_t *, int flags);
	int (*sendmsg)(struct sock_t *, const struct msghdr_t *, int flags);
	int (*connect)(struct sock_t *);
	int (*accept)(struct sock_t *, struct sock_t *);
};

/* inet protocols */
extern struct proto_t udp_proto;
extern struct proto_t tcp_proto;
extern struct proto_t raw_proto;
extern struct proto_t icmp_proto;

#endif
