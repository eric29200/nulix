#include <net/socket.h>
#include <drivers/rtl8139.h>
#include <net/ip.h>
#include <proc/sched.h>
#include <fs/fs.h>
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
 * Socket file operations.
 */
struct file_operations_t socket_fops = {
  .close      = socket_close,
};

/*
 * Create a socket.
 */
int do_socket(int domain, int type, int protocol)
{
  struct socket_t *sock;
  struct file_t *filp;
  int fd;

  /* check protocol */
  if (domain != AF_INET || type != SOCK_DGRAM || protocol != IP_PROTO_ICMP)
    return -EINVAL;

  /* allocate a socket */
  sock = sock_alloc();
  if (!sock)
    return -EMFILE;

  /* set socket */
  sock->dev = rtl8139_get_net_device();
  sock->state = SS_UNCONNECTED;
  sock->type = type;
  sock->protocol = protocol;
  sock->ops = &icmp_prot_ops;
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
 * Send to system call.
 */
int do_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, size_t addrlen)
{
  struct socket_t *sock;

  /* unused flags */
  UNUSED(flags);

  /* check socket file descriptor */
  if (sockfd < 0 || sockfd >= NR_OPEN || current_task->filp[sockfd] == NULL)
    return -EBADF;

  /* find socket */
  sock = sock_lookup(current_task->filp[sockfd]->f_inode);
  if (!sock)
    return -EINVAL;

  /* send message no implemented */
  if (!sock->ops || !sock->ops->sendto)
    return -EINVAL;

  return sock->ops->sendto(sock, buf, len, dest_addr, addrlen);
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

  /* send message no implemented */
  if (!sock->ops || !sock->ops->recvmsg)
    return -EINVAL;

  return sock->ops->recvmsg(sock, msg, flags);
}
