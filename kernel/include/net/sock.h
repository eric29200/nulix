#ifndef _SOCK_H_
#define _SOCK_H_

#include <lib/list.h>
#include <net/socket.h>
#include <net/inet/in.h>
#include <net/unix/un.h>
#include <net/sk_buff.h>

/*
 * UNIX socket.
 */
struct unix_opt_t {
	struct sockaddr_un		sunaddr;
	size_t				sunaddr_len;
	uint16_t			shutdown;
	struct inode_t *		inode;
	struct sock_t *			other;
};

/*
 * Inet socket.
 */
struct inet_opt_t {
	struct net_device_t *		dev;
	struct sockaddr_in		src_sin;
	struct sockaddr_in		dst_sin;
	uint32_t			seq_no;
	uint32_t			ack_no;
	struct proto_t *		prot;
};

/*
 * Internal socket.
 */
struct sock_t {
	uint16_t			protocol;
	struct socket_t *		sock;
	off_t				msg_position;
	union {
		struct unix_opt_t	af_unix;
		struct inet_opt_t	af_inet;
	} protinfo;
	struct list_head_t		list;
	struct list_head_t		skb_list;
};

/*
 * Inet protocol.
 */
struct proto_t {
	int (*handle)(struct sock_t *, struct sk_buff_t *);
	int (*close)(struct sock_t *);
	int (*recvmsg)(struct sock_t *, struct msghdr_t *, int, int);
	int (*sendmsg)(struct sock_t *, const struct msghdr_t *, int, int);
	int (*connect)(struct sock_t *);
	int (*accept)(struct sock_t *, struct sock_t *);
};

/* inet protocols */
extern struct proto_t udp_proto;
extern struct proto_t tcp_proto;
extern struct proto_t raw_proto;
extern struct proto_t icmp_proto;

#endif