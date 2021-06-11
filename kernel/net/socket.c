#include <net/socket.h>
#include <proc/sched.h>
#include <fs/fs.h>
#include <stderr.h>

/* sockets table */
static struct socket_t sockets[NR_SOCKETS];

/*
 * Socket operations.
 */
static struct file_operations_t sock_fops = {
  .open           = NULL,
  .read           = NULL,
  .write          = NULL,
  .getdents64     = NULL,
};

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
 * Create a socket.
 */
int do_socket(int domain, int type, int protocol)
{
  struct socket_t *sock;
  struct file_t *filp;
  int fd;

  /* check protocol */
  if (domain != AF_INET && type != SOCK_DGRAM)
    return -EINVAL;

  /* allocate a socket */
  sock = sock_alloc();
  if (!sock)
    return -EMFILE;

  /* set socket */
  sock->state = SS_UNCONNECTED;
  sock->type = type;
  sock->protocol = protocol;

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
  current_task->filp[fd]->f_op = &sock_fops;

  return fd;
}

/*
 * Send to system call.
 */
int do_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, size_t addrlen)
{
  struct socket_t *sock;

  /* check socket file descriptor */
  if (sockfd < 0 || sockfd >= NR_OPEN || current_task->filp[sockfd] == NULL)
    return -EBADF;

  /* find socket */
  sock = sock_lookup(current_task->filp[sockfd]->f_inode);
  if (!sock)
    return -EINVAL;

  return len;
}
