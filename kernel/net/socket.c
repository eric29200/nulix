#include <net/socket.h>
#include <drivers/rtl8139.h>
#include <net/ip.h>
#include <proc/sched.h>
#include <fs/fs.h>
#include <time.h>
#include <stderr.h>

/* sockets table */
struct socket_t sockets[NR_SOCKETS];

/*
 * Allocate a socket.
 */
static struct socket_t *sock_alloc()
{
  int i;

  /* find a free socket */
  for (i = 0; i < NR_SOCKETS; i++)
    if (sockets[i].state == SS_FREE)
      break;

  /* no free sockets */
  if (i >= NR_SOCKETS)
    return NULL;

  /* reset socket */
  memset(&sockets[i], 0, sizeof(struct socket_t));

  /* get an empty inode */
  sockets[i].inode = get_empty_inode();
  if (!sockets[i].inode) {
    sockets[i].state = SS_FREE;
    return NULL;
  }

  return &sockets[i];
}

/*
 * Release a socket.
 */
static void sock_release(struct socket_t *sock)
{
  struct list_head_t *pos, *n;
  struct sk_buff_t *skb;

  /* free all remaining buffers */
  list_for_each_safe(pos, n, &sock->skb_list) {
    skb = list_entry(pos, struct sk_buff_t, list);
    list_del(&skb->list);
    skb_free(skb);
  }

  /* mark socket free */
  sock->state = SS_FREE;
}

/*
 * Find socket on an inode.
 */
static struct socket_t *sock_lookup(struct inode_t *inode)
{
  int i;

  for (i = 0; i < NR_SOCKETS; i++)
    if (sockets[i].inode == inode)
      return &sockets[i];

  return NULL;
}

/*
 * Close a socket.
 */
int socket_close(struct file_t *filp)
{
  struct socket_t *sock;

  /* get socket */
  sock = sock_lookup(filp->f_inode);
  if (!sock)
    return -EINVAL;

  /* free socket */
  sock_release(sock);

  return 0;
}

/*
 * Poll on a socket.
 */
int socket_poll(struct file_t *filp)
{
  struct socket_t *sock;
  int mask = 0;

  /* get socket */
  sock = sock_lookup(filp->f_inode);
  if (!sock)
    return -EINVAL;

  /* set waiting channel */
  current_task->waiting_chan = &sock->waiting_chan;

  /* check if there is a message in the queue */
  if (!list_empty(&sock->skb_list))
    mask |= POLLIN;

  return mask;
}

/*
 * Socket file operations.
 */
struct file_operations_t socket_fops = {
  .poll       = socket_poll,
  .close      = socket_close,
};

/*
 * Create a socket.
 */
int do_socket(int domain, int type, int protocol)
{
  struct prot_ops *sock_ops;
  struct socket_t *sock;
  struct file_t *filp;
  int fd;

  /* only internet sockets */
  if (domain != AF_INET)
    return -EINVAL;

  /* choose socket type */
  switch (type) {
    case SOCK_DGRAM:
      /* choose protocol */
      switch (protocol) {
        case 0:
        case IP_PROTO_UDP:
          protocol = IP_PROTO_UDP;
          sock_ops = &udp_prot_ops;
          break;
        case IP_PROTO_ICMP:
          sock_ops = &icmp_prot_ops;
          break;
        default:
          return -EINVAL;
      }

      break;
    case SOCK_RAW:
      sock_ops = &raw_prot_ops;
      break;
    default:
      return -EINVAL;
  }

  /* allocate a socket */
  sock = sock_alloc();
  if (!sock)
    return -EMFILE;

  /* set socket */
  sock->dev = rtl8139_get_net_device();
  sock->state = SS_UNCONNECTED;
  sock->type = type;
  sock->protocol = protocol;
  sock->ops = sock_ops;
  INIT_LIST_HEAD(&sock->skb_list);

  /* get a new empty file */
  filp = get_empty_filp();
  if (!filp) {
    sock_release(sock);
    return -EMFILE;
  }

  /* find a free file slot */
  for (fd = 0; fd < NR_FILE; fd++)
    if (!current_task->filp[fd])
      break;

  /* no free slot */
  if (fd >= NR_FILE) {
    filp->f_ref = 0;
    sock_release(sock);
    return -EMFILE;
  }

  /* set file */
  current_task->filp[fd] = filp;
  current_task->filp[fd]->f_mode = 3;
  current_task->filp[fd]->f_flags = 0;
  current_task->filp[fd]->f_pos = 0;
  current_task->filp[fd]->f_ref = 1;
  current_task->filp[fd]->f_inode = sock->inode;
  current_task->filp[fd]->f_op = &socket_fops;

  return fd;
}

/*
 * Bind system call (attach an address to a socket).
 */
int do_bind(int sockfd, const struct sockaddr *addr, size_t addrlen)
{
  uint16_t max_port;
  struct sockaddr_in *sin;
  struct socket_t *sock;
  int i;

  /* unused addrlen */
  UNUSED(addrlen);

  /* check socket file descriptor */
  if (sockfd < 0 || sockfd >= NR_OPEN || current_task->filp[sockfd] == NULL)
    return -EBADF;

  /* find socket */
  sock = sock_lookup(current_task->filp[sockfd]->f_inode);
  if (!sock)
    return -EINVAL;

  /* get internet address */
  sin = (struct sockaddr_in *) addr;

  /* check if asked port is already mapped or find max mapped port */
  for (i = 0, max_port = 0; i < NR_SOCKETS; i++) {
    /* different protocol : skip */
    if (!sockets[i].state || sockets[i].protocol != sock->protocol)
      continue;

    /* already mapped */
    if (sin->sin_port && sin->sin_port == sockets[i].sin.sin_port)
      return -ENOSPC;

    /* update max mapped port */
    if (htons(sockets[i].sin.sin_port) > max_port)
      max_port = htons(sockets[i].sin.sin_port);
  }

  /* allocate a dynamic port */
  if (!sin->sin_port)
    sin->sin_port = htons(max_port < IP_START_DYN_PORT ? IP_START_DYN_PORT : max_port + 1);

  /* copy address */
  memcpy(&sock->sin, sin, sizeof(struct sockaddr));

  return 0;
}

/*
 * Send to system call.
 */
int do_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, size_t addrlen)
{
  struct socket_t *sock;
  struct msghdr_t msg;
  struct iovec_t iovec;

  /* unused address length */
  UNUSED(addrlen);

  /* check socket file descriptor */
  if (sockfd < 0 || sockfd >= NR_OPEN || current_task->filp[sockfd] == NULL)
    return -EBADF;

  /* find socket */
  sock = sock_lookup(current_task->filp[sockfd]->f_inode);
  if (!sock)
    return -EINVAL;

  /* send message not implemented */
  if (!sock->ops || !sock->ops->sendmsg)
    return -EINVAL;

  /* build buffer */
  iovec.iov_base = (void *) buf;
  iovec.iov_len = len;

  /* build message */
  msg.msg_name = (void *) dest_addr;
  msg.msg_namelen = sizeof(struct sockaddr);
  msg.msg_iov = &iovec;
  msg.msg_iovlen = 1;
  msg.msg_control = NULL;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;

  /* send message */
  return sock->ops->sendmsg(sock, &msg, flags);
}

/*
 * Receive from system call.
 */
int do_recvfrom(int sockfd, const void *buf, size_t len, int flags, struct sockaddr *src_addr, size_t addrlen)
{
  struct socket_t *sock;
  struct msghdr_t msg;
  struct iovec_t iovec;

  /* unused address length */
  UNUSED(addrlen);

  /* check socket file descriptor */
  if (sockfd < 0 || sockfd >= NR_OPEN || current_task->filp[sockfd] == NULL)
    return -EBADF;

  /* find socket */
  sock = sock_lookup(current_task->filp[sockfd]->f_inode);
  if (!sock)
    return -EINVAL;

  /* receive message not implemented */
  if (!sock->ops || !sock->ops->recvmsg)
    return -EINVAL;

  /* build buffer */
  iovec.iov_base = (void *) buf;
  iovec.iov_len = len;

  /* build message */
  msg.msg_name = (void *) src_addr;
  msg.msg_namelen = sizeof(struct sockaddr);
  msg.msg_iov = &iovec;
  msg.msg_iovlen = 1;
  msg.msg_control = NULL;
  msg.msg_controllen = 0;
  msg.msg_flags = 0;

  return sock->ops->recvmsg(sock, &msg, flags);
}

/*
 * Receive a message system call.
 */
int do_recvmsg(int sockfd, struct msghdr_t *msg, int flags)
{
  struct socket_t *sock;

  /* check socket file descriptor */
  if (sockfd < 0 || sockfd >= NR_OPEN || current_task->filp[sockfd] == NULL)
    return -EBADF;

  /* find socket */
  sock = sock_lookup(current_task->filp[sockfd]->f_inode);
  if (!sock)
    return -EINVAL;

  /* receive message not implemented */
  if (!sock->ops || !sock->ops->recvmsg)
    return -EINVAL;

  return sock->ops->recvmsg(sock, msg, flags);
}
