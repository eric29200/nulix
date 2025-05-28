#include <fs/fs.h>
#include <net/sock.h>
#include <proc/sched.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>
#include <fcntl.h>
#include <uio.h>

/* socket file operations */
struct file_operations socket_fops;

/*
 * Allocate a socket.
 */
static struct socket *sock_alloc()
{
	struct inode *inode;
	struct socket *sock;

	/* get an empty inode */
	inode = get_empty_inode(NULL);
	if (!inode)
		return NULL;

	/* set inode */
	inode->i_mode = S_IFSOCK;
	inode->i_sock = 1;
	inode->i_uid = current_task->uid;
	inode->i_gid = current_task->gid;

	/* set socket */
	sock = &inode->u.socket_i;
	memset(sock, 0, sizeof(struct socket));
	sock->state = SS_UNCONNECTED;
	sock->inode = inode;

	return sock;
}

/*
 * Init socket data.
 */
void sock_init_data(struct socket *sock)
{
	sock->sk->peercred.pid = 0;
	sock->sk->peercred.uid = -1;
	sock->sk->peercred.gid = -1;
}

/*
 * Release a socket.
 */
static void sock_release(struct socket *sock)
{
	/* release socket */
	if (sock->ops && sock->ops->release)
		sock->ops->release(sock);

	/* release inode */
	iput(sock->inode);
}

/*
 * Get a file slot.
 */
static int get_fd(struct inode *inode)
{
	struct file *filp;
	int fd;

	/* get a file slot */
	fd = get_unused_fd();
	if (fd < 0)
		return fd;

	/* get a new empty file */
	filp = get_empty_filp();
	if (!filp)
		return -EMFILE;

	/* set file */
	current_task->files->filp[fd] = filp;
	FD_CLR(fd, &current_task->files->close_on_exec);
	filp->f_mode = O_RDWR;
	filp->f_flags = 0;
	filp->f_pos = 0;
	filp->f_count = 1;
	filp->f_inode = inode;
	filp->f_op = &socket_fops;

	return fd;
}

/*
 * Find socket on a file descriptor.
 */
static struct socket *sockfd_lookup(int fd, struct file **filpp, int *err)
{
	struct file *filp;

	/* get file */
	filp = fget(fd);
	if (!filp) {
		*err = -EBADF;
		return NULL;
	}

	/* get socket */
	if (!filp->f_inode || !filp->f_inode->i_sock) {
		*err = -ENOTSOCK;
		return NULL;
	}

	/* set filpp */
	if (filpp)
		*filpp = filp;

	return &filp->f_inode->u.socket_i;
}

/*
 * Close a socket.
 */
static int sock_close(struct file *filp)
{
	if (!filp->f_inode)
		return 0;

	/* release socket */
	sock_release(&filp->f_inode->u.socket_i);

	return 0;
}

/*
 * Poll on a socket.
 */
static int sock_poll(struct file *filp, struct select_table *wait)
{
	struct socket *sock;
	int mask = 0;

	/* get socket */
	sock = &filp->f_inode->u.socket_i;
	if (!sock)
		return -EINVAL;

	/* check if there is a message in the queue */
	if (sock->ops && sock->ops->poll)
		mask = sock->ops->poll(sock, wait);

	return mask;
}

/*
 * Ioctl on a socket.
 */
static int sock_ioctl(struct file *filp, int cmd, unsigned long arg)
{
	struct socket *sock;

	/* get socket */
	sock = &filp->f_inode->u.socket_i;
	if (!sock)
		return -EINVAL;

	return sock->ops->ioctl(sock, cmd, arg);
}

/*
 * Socket read.
 */
static int sock_read(struct file *filp, char *buf, int len)
{
	struct socket *sock;
	struct msghdr msg;
	struct iovec iov;

	/* get socket */
	sock = &filp->f_inode->u.socket_i;
	if (!sock)
		return -EINVAL;

	/* receive message not implemented */
	if (!sock->ops || !sock->ops->recvmsg)
		return -EINVAL;

	/* build message */
	memset(&msg, 0, sizeof(struct msghdr));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	iov.iov_base = buf;
	iov.iov_len = len;

	return sock->ops->recvmsg(sock, &msg, filp->f_flags & O_NONBLOCK, 0);
}

/*
 * Socket write.
 */
static int sock_write(struct file *filp, const char *buf, int len)
{
	struct socket *sock;
	struct msghdr msg;
	struct iovec iov;

	/* get socket */
	sock = &filp->f_inode->u.socket_i;
	if (!sock)
		return -EINVAL;

	/* send message not implemented */
	if (!sock->ops || !sock->ops->sendmsg)
		return -EINVAL;

	/* build message */
	memset(&msg, 0, sizeof(struct msghdr));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	iov.iov_base = (char *) buf;
	iov.iov_len = len;

	return sock->ops->sendmsg(sock, &msg, filp->f_flags & O_NONBLOCK, 0);
}

/*
 * Socket file operations.
 */
struct file_operations socket_fops = {
	.read		= sock_read,
	.write		= sock_write,
	.poll		= sock_poll,
	.ioctl		= sock_ioctl,
	.close		= sock_close,
};

/*
 * Create a socket.
 */
int sys_socket(int domain, int type, int protocol)
{

	struct prot_ops *sock_ops;
	struct socket *sock;
	int err, fd;

	/* choose protocol operations */
	switch (domain) {
		case AF_INET:
			sock_ops = &inet_ops;
			break;
		case AF_UNIX:
			sock_ops = &unix_ops;
			break;
		default:
			return -EINVAL;
	}

	/* allocate a socket */
	sock = sock_alloc();
	if (!sock)
		return -EMFILE;

	/* set socket */
	sock->state = SS_UNCONNECTED;
	sock->family = domain;
	sock->type = type;
	sock->ops = sock_ops;

	/* create not implemented */
	if (!sock->ops || !sock->ops->create) {
		sock_release(sock);
		return -EINVAL;
	}

	/* create socket */
	err = sock->ops->create(sock, protocol);
	if (err) {
		sock_release(sock);
		return err;
	}

	/* get a file slot */
	fd = get_fd(sock->inode);
	if (fd < 0) {
		sock_release(sock);
		return -EINVAL;
	}

	return fd;
}

/*
 * Bind system call (attach an address to a socket).
 */
int sys_bind(int sockfd, const struct sockaddr *addr, size_t addrlen)
{
	struct socket *sock;
	int err;

	/* find socket */
	sock = sockfd_lookup(sockfd, NULL, &err);
	if (!sock)
		return err;

	/* bind not implemented */
	if (!sock->ops || !sock->ops->bind)
		return -EINVAL;

	return sock->ops->bind(sock, addr, addrlen);
}

/*
 * Connect system call.
 */
int sys_connect(int sockfd, const struct sockaddr *addr, size_t addrlen)
{
	struct socket *sock;
	int err;

	/* find socket */
	sock = sockfd_lookup(sockfd, NULL, &err);
	if (!sock)
		return err;

	/* connect not implemented */
	if (!sock->ops || !sock->ops->connect)
		return -EINVAL;

	return sock->ops->connect(sock, addr, addrlen);
}

/*
 * Listen system call.
 */
int sys_listen(int sockfd, int backlog)
{
	struct socket *sock;
	int err;

	/* unused backlog */
	UNUSED(backlog);

	/* find socket */
	sock = sockfd_lookup(sockfd, NULL, &err);
	if (!sock)
		return err;

	/* listen not implemented */
	if (!sock->ops || !sock->ops->listen)
		return -EINVAL;

	return sock->ops->listen(sock);
}

/*
 * Accept system call.
 */
int sys_accept(int sockfd, struct sockaddr *addr, size_t *addrlen)
{
	struct socket *sock, *new_sock;
	int fd, err;

	/* unused address length */
	UNUSED(addrlen);

	/* find socket */
	sock = sockfd_lookup(sockfd, NULL, &err);
	if (!sock)
		return err;

	/* allocate a new socket */
	new_sock = sock_alloc();
	if (!new_sock)
		return -ENOSR;

	/* duplicate socket */
	new_sock->type = sock->type;
	new_sock->ops = sock->ops;
	err = sock->ops->dup(sock, new_sock);
	if (err < 0) {
		sock_release(new_sock);
		return err;
	}

	/* accept not implemented */
	if (!new_sock->ops || !new_sock->ops->accept) {
		sock_release(new_sock);
		return -EINVAL;
	}

	/* call accept protocol */
	err = new_sock->ops->accept(sock, new_sock, addr);
	if (err < 0) {
		sock_release(new_sock);
		return err;
	}

	/* get a file slot */
	fd = get_fd(new_sock->inode);
	if (fd < 0) {
		sock_release(new_sock);
		return -EINVAL;
	}

	return fd;
}

/*
 * Send system call.
 */
int sys_send(int sockfd, const void * buf, size_t len, int flags)
{
	return sys_sendto(sockfd, buf, len, flags, NULL, 0);
}

/*
 * Send to system call.
 */
int sys_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, size_t addrlen)
{
	struct socket *sock;
	struct iovec iovec;
	struct msghdr msg;
	struct file *filp;
	int err;

	/* unused address length */
	UNUSED(addrlen);

	/* find socket */
	sock = sockfd_lookup(sockfd, &filp, &err);
	if (!sock)
		return err;

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
	return sock->ops->sendmsg(sock, &msg, filp->f_flags & O_NONBLOCK, flags);
}

/*
 * Send a message system call.
 */
int sys_sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
	struct socket *sock;
	struct file *filp;
	int err;

	/* find socket */
	sock = sockfd_lookup(sockfd, &filp, &err);
	if (!sock)
		return err;

	/* send message not implemented */
	if (!sock->ops || !sock->ops->sendmsg)
		return -EINVAL;

	return sock->ops->sendmsg(sock, msg, filp->f_flags & O_NONBLOCK, flags);
}

/*
 * Receive system call.
 */
int sys_recv(int sockfd, void *buf, size_t size, int flags)
{
	return sys_recvfrom(sockfd, buf, size, flags, NULL, NULL);
}

/*
 * Receive from system call.
 */
int sys_recvfrom(int sockfd, const void *buf, size_t len, int flags, struct sockaddr *src_addr, size_t *addrlen)
{
	struct socket *sock;
	struct iovec iovec;
	struct msghdr msg;
	struct file *filp;
	int err;

	/* unused address length */
	UNUSED(addrlen);

	/* find socket */
	sock = sockfd_lookup(sockfd, &filp, &err);
	if (!sock)
		return err;

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

	return sock->ops->recvmsg(sock, &msg, filp->f_flags & O_NONBLOCK, flags);
}

/*
 * Receive a message system call.
 */
int sys_recvmsg(int sockfd, struct msghdr *msg, int flags)
{
	struct socket *sock;
	struct file *filp;
	int err;

	/* find socket */
	sock = sockfd_lookup(sockfd, &filp, &err);
	if (!sock)
		return err;

	/* receive message not implemented */
	if (!sock->ops || !sock->ops->recvmsg)
		return -EINVAL;

	return sock->ops->recvmsg(sock, msg, filp->f_flags & O_NONBLOCK, flags);
}

/*
 * Shutdown system call.
 */
int sys_shutdown(int sockfd, int how)
{
	struct socket *sock;
	int err;

	/* find socket */
	sock = sockfd_lookup(sockfd, NULL, &err);
	if (!sock)
		return err;

	/* shutdown not implemented */
	if (!sock->ops || !sock->ops->shutdown)
		return -EINVAL;

	return sock->ops->shutdown(sock, how);
}

/*
 * Get peer name system call.
 */
int sys_getpeername(int sockfd, struct sockaddr *addr, size_t *addrlen)
{
	struct socket *sock;
	int err;

	/* find socket */
	sock = sockfd_lookup(sockfd, NULL, &err);
	if (!sock)
		return err;

	/* getpeername not implemented */
	if (!sock->ops || !sock->ops->getpeername)
		return -EINVAL;

	return sock->ops->getpeername(sock, addr, addrlen);
}

/*
 * Get sock name system call.
 */
int sys_getsockname(int sockfd, struct sockaddr *addr, size_t *addrlen)
{
	struct socket *sock;
	int err;

	/* find socket */
	sock = sockfd_lookup(sockfd, NULL, &err);
	if (!sock)
		return err;

	/* getsockname not implemented */
	if (!sock->ops || !sock->ops->getsockname)
		return -EINVAL;

	return sock->ops->getsockname(sock, addr, addrlen);
}

/*
 * Get socket options.
 */
static int sock_getsockopt(struct socket *sock, int optname, void *optval, size_t *optlen)
{
	size_t len;

	/* check length */
	if (!optlen)
		return -EINVAL;
	len = *optlen;

	switch (optname) {
		case SO_SNDBUF:
			*((int *) optval) = sock->sk->sndbuf;
			break;
		case SO_PEERCRED:
		 	len = sizeof(struct ucred) < len ? sizeof(struct ucred) : len;
			memcpy(optval, &sock->sk->peercred, len);
			break;
		default:
			printf("sock_getsockopt(%d) undefined\n", optname);
			break;
	}

	/* fix length */
	*optlen = len;

	return 0;
}

/*
 * Set socket options.
 */
static int sock_setsockopt(struct socket *sock, int optname, void *optval, size_t optlen)
{
	UNUSED(optlen);

	switch (optname) {
		case SO_SNDBUF:
			sock->sk->sndbuf = *((int *) optval);
			break;
		default:
			printf("sock_setsockopt(%d) undefined\n", optname);
			break;
	}

	return 0;
}

/*
 * Get socket options system call.
 */
int sys_getsockopt(int sockfd, int level, int optname, void *optval, size_t *optlen)
{
	struct socket *sock;
	int err;

	/* find socket */
	sock = sockfd_lookup(sockfd, NULL, &err);
	if (!sock)
		return err;

	/* socket options */
	if (level == SOL_SOCKET)
		return sock_getsockopt(sock, optname, optval, optlen);

	/* setsockopt not implemented */
	if (!sock->ops || !sock->ops->getsockopt)
		return -EINVAL;

	return sock->ops->getsockopt(sock, level, optname, optval, optlen);
}

/*
 * Set socket options system call.
 */
int sys_setsockopt(int sockfd, int level, int optname, void *optval, size_t optlen)
{
	struct socket *sock;
	int err;

	/* find socket */
	sock = sockfd_lookup(sockfd, NULL, &err);
	if (!sock)
		return err;

	/* socket options */
	if (level == SOL_SOCKET)
		return sock_setsockopt(sock, optname, optval, optlen);

	/* setsockopt not implemented */
	if (!sock->ops || !sock->ops->setsockopt)
		return -EINVAL;

	return sock->ops->setsockopt(sock, level, optname, optval, optlen);
}

/*
 * Socketcall system call.
 */
int sys_socketcall(int call, unsigned long *args)
{
	int err;

	/* check call */
	if(call < 1|| call > SYS_RECVMSG)
		return -EINVAL;

	switch(call) {
		case SYS_SOCKET:
			err = sys_socket(args[0], args[1], args[2]);
			break;
		case SYS_BIND:
			err = sys_bind(args[0], (struct sockaddr *) args[1], args[2]);
			break;
		case SYS_CONNECT:
			err = sys_connect(args[0], (struct sockaddr *) args[1], args[2]);
			break;
		case SYS_LISTEN:
			err = sys_listen(args[0], args[1]);
			break;
		case SYS_ACCEPT:
			err = sys_accept(args[0], (struct sockaddr *) args[1], (size_t *) args[2]);
			break;
		case SYS_GETSOCKNAME:
			err = sys_getsockname(args[0],(struct sockaddr *) args[1], (size_t *) args[2]);
			break;
		case SYS_GETPEERNAME:
			err = sys_getpeername(args[0], (struct sockaddr *) args[1], (size_t *) args[2]);
			break;
		case SYS_SEND:
			err = sys_send(args[0], (void *) args[1], args[2], args[3]);
			break;
		case SYS_SENDTO:
			err = sys_sendto(args[0],(void *) args[1], args[2], args[3], (struct sockaddr *) args[4], args[5]);
			break;
		case SYS_RECV:
			err = sys_recv(args[0], (void *) args[1], args[2], args[3]);
			break;
		case SYS_RECVFROM:
			err = sys_recvfrom(args[0], (void *) args[1], args[2], args[3], (struct sockaddr *) args[4], (size_t *) args[5]);
			break;
		case SYS_SHUTDOWN:
			err = sys_shutdown(args[0], args[1]);
			break;
		case SYS_SETSOCKOPT:
			err = sys_setsockopt(args[0], args[1], args[2], (char *) args[3], args[4]);
			break;
		case SYS_GETSOCKOPT:
			err = sys_getsockopt(args[0], args[1], args[2], (char *) args[3], (size_t *) args[4]);
			break;
		case SYS_SENDMSG:
			err = sys_sendmsg(args[0], (struct msghdr *) args[1], args[2]);
			break;
		case SYS_RECVMSG:
			err = sys_recvmsg(args[0], (struct msghdr *) args[1], args[2]);
			break;
		default:
			err = -EINVAL;
			break;
	}
	return err;
}
