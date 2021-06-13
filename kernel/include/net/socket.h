#ifndef _SOCKET_H_
#define _SOCKET_H_

#include <stddef.h>
#include <net/net.h>
#include <uio.h>

#define NR_SOCKETS      32

/* addresses families */
#define AF_UNIX         1
#define AF_INET         2

/* protocol families */
#define PF_UNIX         1
#define PF_INET         2

/* socket types */
#define SOCK_STREAM     1
#define SOCK_DGRAM      2
#define SOCK_RAW        3

/*
 * Socket address.
 */
struct sockaddr {
  uint16_t    sa_family;
  char        sa_data[14];
};

/*
 * Inet socker address.
 */
struct sockaddr_in {
  uint16_t    sin_family;
  uint16_t    sin_port;
  uint32_t    sin_addr;
  uint8_t     sin_pad[8];
};

/*
 * Message header.
 */
struct msghdr_t {
  void *            msg_name;
  size_t            msg_namelen;
  struct iovec_t *  msg_iov;
  size_t            msg_iovlen;
  void *            msg_control;
  size_t            msg_controllen;
  int               msg_flags;
};

/*
 * Socket state.
 */
typedef enum {
  SS_FREE = 0,
  SS_UNCONNECTED,
  SS_CONNECTING,
  SS_CONNECTED,
  SS_DISCONNECTING
} socket_state_t;

/*
 * Socket structure.
 */
struct socket_t {
  struct net_device_t * dev;
  socket_state_t        state;
  uint16_t              protocol;
  uint16_t              type;
  struct inode_t *      inode;
  struct prot_ops *     ops;
};

/*
 * Protocol operations.
 */
struct prot_ops {
  int (*sendto)(struct socket_t *, const void *, size_t, const struct sockaddr *, size_t);
  int (*recvmsg)(struct socket_t *, struct msghdr_t *, int);
};

/* protocol operations */
extern struct prot_ops icmp_prot_ops;

/* socket system calls */
int do_socket(int domain, int type, int protocol);
int do_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, size_t addrlen);
int do_recvmsg(int sockfd, struct msghdr_t *msg, int flags);

#endif
