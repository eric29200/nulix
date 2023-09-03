#ifndef _SOCKET_H_
#define _SOCKET_H_

#include <proc/wait.h>
#include <stddef.h>

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

/* flags for send/recv */
#define MSG_OOB			1
#define MSG_PEEK		2

/* flags for shutdown */
#define RCV_SHUTDOWN		1
#define SEND_SHUTDOWN		2
#define SHUTDOWN_MASK		3

/* socket options */
#define SOL_SOCKET		1
#define SO_DEBUG		1
#define SO_REUSEADDR		2
#define SO_TYPE			3
#define SO_ERROR		4
#define SO_DONTROUTE		5
#define SO_BROADCAST		6
#define SO_SNDBUF		7
#define SO_RCVBUF		8
#define SO_SNDBUFFORCE		32
#define SO_RCVBUFFORCE		33
#define SO_KEEPALIVE		9
#define SO_OOBINLINE		10
#define SO_NO_CHECK		11
#define SO_PRIORITY		12
#define SO_LINGER		13
#define SO_BSDCOMPAT		14
#define SO_PASSCRED		16
#define SO_PEERCRED		17
#define SO_RCVLOWAT		18
#define SO_SNDLOWAT		19
#define SO_RCVTIMEO		20
#define SO_SNDTIMEO		21

/* system calls */
#define SYS_SOCKET		1
#define SYS_BIND		2
#define SYS_CONNECT		3
#define SYS_LISTEN		4
#define SYS_ACCEPT		5
#define SYS_GETSOCKNAME		6
#define SYS_GETPEERNAME		7
#define SYS_SOCKETPAIR		8
#define SYS_SEND		9
#define SYS_RECV		10
#define SYS_SENDTO		11
#define SYS_RECVFROM		12
#define SYS_SHUTDOWN		13
#define SYS_SETSOCKOPT		14
#define SYS_GETSOCKOPT		15
#define SYS_SENDMSG		16
#define SYS_RECVMSG		17

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
struct msghdr {
	void *			msg_name;
	size_t			msg_namelen;
	struct iovec *		msg_iov;
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
struct socket {
	uint16_t		family;
	uint16_t		type;
	socket_state_t		state;
	struct prot_ops *	ops;
	struct wait_queue *	wait;
	struct inode *		inode;
	struct sock *		sk;
};

/*
 * Protocol operations.
 */
struct prot_ops {
	int (*create)(struct socket *, int);
	int (*dup)(struct socket *, struct socket *);
	int (*release)(struct socket *);
	int (*poll)(struct socket *, struct select_table *);
	int (*ioctl)(struct socket *, int, unsigned long);
	int (*recvmsg)(struct socket *, struct msghdr *, int, int);
	int (*sendmsg)(struct socket *, const struct msghdr *, int, int);
	int (*bind)(struct socket *, const struct sockaddr *, size_t);
	int (*accept)(struct socket *, struct socket *, struct sockaddr *);
	int (*connect)(struct socket *, const struct sockaddr *, size_t);
	int (*shutdown)(struct socket *, int);
	int (*getpeername)(struct socket *, struct sockaddr *, size_t *);
	int (*getsockname)(struct socket *, struct sockaddr *, size_t *);
	int (*getsockopt)(struct socket *, int, int, void *, size_t *);
	int (*setsockopt)(struct socket *, int, int, void *, size_t);
};

/* protocole operations */
extern struct prot_ops inet_ops;
extern struct prot_ops unix_ops;

/* socket system calls */
int sys_socket(int domain, int type, int protocol);
int sys_bind(int sockfd, const struct sockaddr *addr, size_t addrlen);
int sys_connect(int sockfd, const struct sockaddr *addr, size_t addrlen);
int sys_listen(int sockfd, int backlog);
int sys_accept(int sockfd, struct sockaddr *addr, size_t *addrlen);
int sys_send(int sockfd, const void * buf, size_t len, int flags);
int sys_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, size_t addrlen);
int sys_sendmsg(int sockfd, const struct msghdr *msg, int flags);
int sys_recv(int sockfd, void *buf, size_t size, int flags);
int sys_recvfrom(int sockfd, const void *buf, size_t len, int flags, struct sockaddr *src_addr, size_t *addrlen);
int sys_recvmsg(int sockfd, struct msghdr *msg, int flags);
int sys_shutdown(int sockfd, int how);
int sys_getpeername(int sockfd, struct sockaddr *addr, size_t *addrlen);
int sys_getsockname(int sockfd, struct sockaddr *addr, size_t *addrlen);
int sys_getsockopt(int sockfd, int level, int optname, void *optval, size_t *optlen);
int sys_setsockopt(int sockfd, int level, int optname, void *optval, size_t optlen);
int sys_socketcall(int call, unsigned long *args);

#endif
