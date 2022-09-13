#ifndef _SOCKET_H_
#define _SOCKET_H_

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
	SS_DISCONNECTING,
	SS_DEAD
} socket_state_t;

/*
 * Socket structure.
 */
struct socket_t {
	uint16_t		family;
	uint16_t		type;
	socket_state_t		state;
	struct prot_ops *	ops;
	int			waiting_chan;
	struct inode_t *	inode;
	void *			data;
};

/*
 * Protocol operations.
 */
struct prot_ops {
	int (*create)(struct socket_t *, int);
	int (*dup)(struct socket_t *, struct socket_t *);
	int (*release)(struct socket_t *);
	int (*close)(struct socket_t *);
	int (*poll)(struct socket_t *);
	int (*recvmsg)(struct socket_t *, struct msghdr_t *, int flags);
	int (*sendmsg)(struct socket_t *, const struct msghdr_t *, int flags);
	int (*bind)(struct socket_t *, const struct sockaddr *, size_t);
	int (*accept)(struct socket_t *, struct socket_t *, struct sockaddr *);
	int (*connect)(struct socket_t *, const struct sockaddr *);
	int (*getpeername)(struct socket_t *, struct sockaddr *, size_t *);
	int (*getsockname)(struct socket_t *, struct sockaddr *, size_t *);
};

/* protocole operations */
extern struct prot_ops inet_ops;
extern struct prot_ops unix_ops;

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
