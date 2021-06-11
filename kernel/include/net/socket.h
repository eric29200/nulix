#ifndef _SOCKET_H_
#define _SOCKET_H_

#include <stddef.h>
#include <net/net.h>

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
};

int do_socket(int domain, int type, int protocol);
int do_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, size_t addrlen);

#endif
