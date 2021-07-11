#include <net/socket.h>
#include <drivers/rtl8139.h>
#include <net/ip.h>
#include <proc/sched.h>
#include <fs/fs.h>
#include <sys/syscall.h>
#include <time.h>
#include <stderr.h>

/* sockets table and free port index */
struct socket_t sockets[NR_SOCKETS];
static uint16_t next_port = IP_START_DYN_PORT;

/*
 * Get next free port.
 */
static uint16_t get_next_free_port()
{
  uint16_t port = next_port;
  next_port++;
  return port;
}

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
 * Socket read.
 */
int socket_read(struct file_t *filp, char *buf, int len)
{
  struct socket_t *sock;
  struct msghdr_t msg;
  struct iovec_t iov;

  /* get socket */
  sock = sock_lookup(filp->f_inode);
  if (!sock)
    return -EINVAL;

  /* receive message not implemented */
  if (!sock->ops || !sock->ops->recvmsg)
    return -EINVAL;

  /* build message */
  memset(&msg, 0, sizeof(struct msghdr_t));
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  iov.iov_base = buf;
  iov.iov_len = len;

  return sock->ops->recvmsg(sock, &msg, 0);
}

/*
 * Socket write.
 */
int socket_write(struct file_t *filp, const char *buf, int len)
{
  struct socket_t *sock;
  struct msghdr_t msg;
  struct iovec_t iov;

  /* get socket */
  sock = sock_lookup(filp->f_inode);
  if (!sock)
    return -EINVAL;

  /* send message not implemented */
  if (!sock->ops || !sock->ops->sendmsg)
    return -EINVAL;

  /* build message */
  memset(&msg, 0, sizeof(struct msghdr_t));
  msg.msg_iov = &iov;
  msg.msg_iovlen = 1;
  iov.iov_base = (char *) buf;
  iov.iov_len = len;

  return sock->ops->sendmsg(sock, &msg, 0);
}

/*
 * Socket file operations.
 */
struct file_operations_t socket_fops = {
  .read       = socket_read,
  .write      = socket_write,
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
    case SOCK_STREAM:
      switch (protocol) {
        case 0:
        case IP_PROTO_TCP:
          protocol = IP_PROTO_TCP;
          sock_ops = &tcp_prot_ops;
          break;
        default:
          return -EINVAL;
      }

      break;
    case SOCK_DGRAM:
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
  sock->family = domain;
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
  struct sockaddr_in *src_sin;
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
  src_sin = (struct sockaddr_in *) addr;

  /* check if asked port is already mapped */
  for (i = 0; i < NR_SOCKETS; i++) {
    /* different protocol : skip */
    if (!sockets[i].state || sockets[i].protocol != sock->protocol)
      continue;

    /* already mapped */
    if (src_sin->sin_port && src_sin->sin_port == sockets[i].src_sin.sin_port)
      return -ENOSPC;
  }

  /* allocate a dynamic port */
  if (!src_sin->sin_port)
    src_sin->sin_port = htons(get_next_free_port());

  /* copy address */
  memcpy(&sock->src_sin, src_sin, sizeof(struct sockaddr));

  return 0;
}

/*
 * Connect system call.
 */
int do_connect(int sockfd, const struct sockaddr *addr, size_t addrlen)
{
  struct socket_t *sock;

  /* unused addrlen */
  UNUSED(addrlen);

  /* check socket file descriptor */
  if (sockfd < 0 || sockfd >= NR_OPEN || current_task->filp[sockfd] == NULL)
    return -EBADF;

  /* find socket */
  sock = sock_lookup(current_task->filp[sockfd]->f_inode);
  if (!sock)
    return -EINVAL;

  /* allocate a dynamic port */
  sock->src_sin.sin_port = htons(get_next_free_port());

  /* copy address */
  memcpy(&sock->dst_sin, addr, sizeof(struct sockaddr));

  /* connect not implemented */
  if (!sock->ops || !sock->ops->connect)
    return -EINVAL;

  return sock->ops->connect(sock);
}

/*
 * Listen system call.
 */
int do_listen(int sockfd, int backlog)
{
  struct socket_t *sock;

  /* unused backlog */
  UNUSED(backlog);

  /* check socket file descriptor */
  if (sockfd < 0 || sockfd >= NR_OPEN || current_task->filp[sockfd] == NULL)
    return -EBADF;

  /* find socket */
  sock = sock_lookup(current_task->filp[sockfd]->f_inode);
  if (!sock)
    return -EINVAL;

  /* update socket state */
  sock->state = SS_LISTENING;

  return 0;
}

/*
 * Accept system call.
 */
int do_accept(int sockfd, struct sockaddr *addr, size_t addrlen)
{
  struct socket_t *sock, *new_sock;
  int new_sockfd, ret;

  /* unused address length */
  UNUSED(addrlen);

  /* check socket file descriptor */
  if (sockfd < 0 || sockfd >= NR_OPEN || current_task->filp[sockfd] == NULL)
    return -EBADF;

  /* find socket */
  sock = sock_lookup(current_task->filp[sockfd]->f_inode);
  if (!sock)
    return -EINVAL;

  /* create a new socket */
  new_sockfd = do_socket(sock->family, sock->type, sock->protocol);
  if (new_sockfd < 0)
    return new_sockfd;

  /* get new socket */
  new_sock = sock_lookup(current_task->filp[new_sockfd]->f_inode);
  if (!new_sock)
    return -EINVAL;

  /* accept not implemented */
  if (!new_sock->ops || !new_sock->ops->accept) {
    sys_close(new_sockfd);
    return -EINVAL;
  }

  /* call accept protocol */
  ret = new_sock->ops->accept(sock, new_sock);
  if (ret < 0) {
    sys_close(new_sockfd);
    return ret;
  }

  /* set accepted address */
  if (addr)
    memcpy(addr, &new_sock->dst_sin, sizeof(struct sockaddr));

  return new_sockfd;
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

/*
 * Get peer name system call.
 */
int do_getpeername(int sockfd, struct sockaddr *addr, size_t *addrlen)
{
  struct sockaddr_in *sin;
  struct socket_t *sock;

  /* unused address length */
  UNUSED(addrlen);

  /* check socket file descriptor */
  if (sockfd < 0 || sockfd >= NR_OPEN || current_task->filp[sockfd] == NULL)
    return -EBADF;

  /* find socket */
  sock = sock_lookup(current_task->filp[sockfd]->f_inode);
  if (!sock)
    return -EINVAL;

  /* copy destination address */
  memset(addr, 0, sizeof(struct sockaddr));
  sin = (struct sockaddr_in *) addr;
  sin->sin_family = AF_INET;
  sin->sin_port = sock->dst_sin.sin_port;
  sin->sin_addr = sock->dst_sin.sin_addr;
  *addrlen = sizeof(struct sockaddr_in);

  return 0;
}
