#ifndef _SOCKET_H_
#define _SOCKET_H_

#include <net/net.h>
#include <lib/list.h>
#include <uio.h>
#include <stddef.h>

#define NR_SOCKETS		32

/* addresses families */
#define AF_UNIX		 	1
#define AF_INET			2

/* protocol families */
#define PF_UNIX			1
#define PF_INET			2

/* socket types */
#define SOCK_STREAM		1
#define SOCK_DGRAM		2
#define SOCK_RAW		3

/*
 * Socket address.
 */
struct sockaddr {
	uint16_t		sa_family;
	char			sa_data[14];
};

/*
 * Inet socker address.
 */
struct sockaddr_in {
	uint16_t		sin_family;
	uint16_t		sin_port;
	uint32_t		sin_addr;
	uint8_t		 	sin_pad[8];
};

/*
 * Message header.
 */
struct msghdr_t {
	void *			msg_name;
	size_t			msg_namelen;
	struct iovec_t *	msg_iov;
	size_t			msg_iovlen;
	void *			msg_control;
	size_t			msg_controllen;
	int			msg_flags;
};

/*
 * Socket state.
 */
typedef enum {
	SS_FREE = 0,
	SS_UNCONNECTED,
	SS_LISTENING,
	SS_CONNECTING,
	SS_CONNECTED,
	SS_DISCONNECTING
} socket_state_t;

/*
 * Socket structure.
 */
struct socket_t {
	struct net_device_t *	dev;
	socket_state_t		state;
	uint16_t		family;
	uint16_t		protocol;
	uint16_t		type;
	struct inode_t *	inode;
	struct sockaddr_in	src_sin;
	struct sockaddr_in	dst_sin;
	struct prot_ops *	ops;
	int			waiting_chan;
	uint32_t		seq_no;
	uint32_t		ack_no;
	off_t			msg_position;
	struct list_head_t	skb_list;
};

/*
 * Protocol operations.
 */
struct prot_ops {
	int (*handle)(struct socket_t *, struct sk_buff_t *);
	int (*recvmsg)(struct socket_t *, struct msghdr_t *, int);
	int (*sendmsg)(struct socket_t *, const struct msghdr_t *, int);
	int (*connect)(struct socket_t *);
	int (*accept)(struct socket_t *, struct socket_t *);
	int (*close)(struct socket_t *);
};

/* protocol operations */
extern struct prot_ops tcp_prot_ops;
extern struct prot_ops udp_prot_ops;
extern struct prot_ops icmp_prot_ops;
extern struct prot_ops raw_prot_ops;

/* socket system calls */
int do_socket(int domain, int type, int protocol);
int do_bind(int sockfd, const struct sockaddr *addr, size_t addrlen);
int do_connect(int sockfd, const struct sockaddr *addr, size_t addrlen);
int do_listen(int sockfd, int backlog);
int do_accept(int sockfd, struct sockaddr *addr, size_t addrlen);
int do_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, size_t addrlen);
int do_recvfrom(int sockfd, const void *buf, size_t len, int flags, struct sockaddr *src_addr, size_t addrlen);
int do_recvmsg(int sockfd, struct msghdr_t *msg, int flags);
int do_getpeername(int sockfd, struct sockaddr *addr, size_t *addrlen);
int do_getsockname(int sockfd, struct sockaddr *addr, size_t *addrlen);

#endif
