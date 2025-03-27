#ifndef _SOCK_H_
#define _SOCK_H_

#include <lib/list.h>
#include <net/socket.h>
#include <net/inet/in.h>
#include <net/unix/un.h>
#include <net/sk_buff.h>

/*
 * Socket credentials.
 */
struct ucred {
	pid_t				pid;
	uid_t				uid;
	gid_t				gid;
};

/*
 * UNIX socket.
 */
struct unix_opt {
	struct sockaddr_un		sunaddr;
	size_t				sunaddr_len;
	uint16_t			shutdown;
	struct inode *			inode;
	struct sock *			other;
};

/*
 * Inet socket.
 */
struct inet_opt {
	struct net_device *		dev;
	struct sockaddr_in		src_sin;
	struct sockaddr_in		dst_sin;
	uint32_t			seq_no;
	uint32_t			ack_no;
	struct proto *			prot;
};

/*
 * Internal socket.
 */
struct sock {
	uint16_t			protocol;
	struct socket *			sock;
	off_t				msg_position;
	size_t				rcvbuf;
	size_t				sndbuf;
	struct ucred 			peercred;
	union {
		struct unix_opt		af_unix;
		struct inet_opt		af_inet;
	} protinfo;
	struct list_head		list;
	struct list_head		skb_list;
};

/*
 * Inet protocol.
 */
struct proto {
	int (*handle)(struct sock *, struct sk_buff *);
	int (*close)(struct sock *);
	int (*recvmsg)(struct sock *, struct msghdr *, int, int);
	int (*sendmsg)(struct sock *, const struct msghdr *, int, int);
	int (*connect)(struct sock *);
	int (*accept)(struct sock *, struct sock *);
};

/* inet protocols */
extern struct proto udp_proto;
extern struct proto tcp_proto;
extern struct proto raw_proto;
extern struct proto icmp_proto;

struct sk_buff *sock_alloc_send_skb(struct socket *sock, size_t len);

#endif
