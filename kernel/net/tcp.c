#include <net/tcp.h>
#include <net/socket.h>

/*
 * TCP protocol operations.
 */
struct prot_ops tcp_prot_ops = {
  .handle       = NULL,
  .recvmsg      = NULL,
  .sendmsg      = NULL,
};
