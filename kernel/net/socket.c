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
	inode->i_uid = current_task->fsuid;
	inode->i_gid = current_task->fsgid;

	/* set socket */
	sock = &inode->u.socket_i;
	memset(sock, 0, sizeof(struct socket));
	sock->state = SS_UNCONNECTED;
	sock->inode = inode;
	sock->file = NULL;

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
	sock->file = NULL;
	iput(sock->inode);
}

/*
 * Get a file slot.
 */
static int get_fd(struct inode *inode)
{
	struct dentry *dentry;
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

	/* allocate a dentry */
	dentry = d_alloc_root(inode);
	if (!dentry) {
		close_fp(filp);
		return -ENOMEM;
	}

	/* set file */
	current_task->files->filp[fd] = filp;
	FD_CLR(fd, &current_task->files->close_on_exec);
	filp->f_mode = O_RDWR;
	filp->f_flags = 0;
	filp->f_pos = 0;
	filp->f_count = 1;
	filp->f_dentry = dentry;
	filp->f_op = &socket_fops;

	return fd;
}

/*
 * Find socket on a file descriptor.
 */
static struct socket *sockfd_lookup(int fd, int *err)
{
	struct inode *inode;
	struct socket *sock;
	struct file *filp;

	/* get file */
	filp = fget(fd);
	if (!filp) {
		*err = -EBADF;
		return NULL;
	}

	/* get inode */
	inode = filp->f_dentry->d_inode;

	/* get socket */
	if (!inode || !inode->i_sock) {
		*err = -ENOTSOCK;
		fput(filp);
		return NULL;
	}

	/* get socket */
	sock = &inode->u.socket_i;

	/* check socket file */
	if (sock->file != filp) {
		printf("sockfd_lookup: socket file changed\n");
		sock->file = filp;
	}

	return sock;
}

/*
 * Release a socket file.
 */
static void sockfd_put(struct socket *sock)
{
	fput(sock->file);
}

/*
 * Close a socket.
 */
static int sock_close(struct file *filp)
{
	struct dentry *dentry;
	struct inode *inode;

	/* get dentry */
	dentry = filp->f_dentry;
	if (!dentry)
		return 0;

	/* get inode */
	inode = dentry->d_inode;
	if (!inode)
		return 0;

	/* release socket */
	sock_release(&inode->u.socket_i);

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
	sock = &filp->f_dentry->d_inode->u.socket_i;
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
	sock = &filp->f_dentry->d_inode->u.socket_i;
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
	sock = &filp->f_dentry->d_inode->u.socket_i;
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
	sock = &filp->f_dentry->d_inode->u.socket_i;
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
	int ret, fd;

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
	ret = sock->ops->create(sock, protocol);
	if (ret) {
		sock_release(sock);
		return ret;
	}

	/* get a file slot */
	fd = get_fd(sock->inode);
	if (fd < 0) {
		sock_release(sock);
		return -EINVAL;
	}

	/* set socket file */
	sock->file = current_task->files->filp[fd];

	return fd;
}

/*
 * Bind system call (attach an address to a socket).
 */
int sys_bind(int sockfd, const struct sockaddr *addr, size_t addrlen)
{
	struct socket *sock;
	int ret;

	/* find socket */
	sock = sockfd_lookup(sockfd, &ret);
	if (!sock)
		return ret;

	/* bind not implemented */
	ret = -EINVAL;
	if (!sock->ops || !sock->ops->bind)
		goto out;

	ret = sock->ops->bind(sock, addr, addrlen);
out:
	sockfd_put(sock);
	return ret;
}

/*
 * Connect system call.
 */
int sys_connect(int sockfd, const struct sockaddr *addr, size_t addrlen)
{
	struct socket *sock;
	int ret;

	/* find socket */
	sock = sockfd_lookup(sockfd, &ret);
	if (!sock)
		return ret;

	/* connect not implemented */
	ret = -EINVAL;
	if (!sock->ops || !sock->ops->connect)
		goto out;

	ret = sock->ops->connect(sock, addr, addrlen);
out:
	sockfd_put(sock);
	return ret;
}

/*
 * Listen system call.
 */
int sys_listen(int sockfd, int backlog)
{
	struct socket *sock;
	int ret;

	/* unused backlog */
	UNUSED(backlog);

	/* find socket */
	sock = sockfd_lookup(sockfd, &ret);
	if (!sock)
		return ret;

	/* listen not implemented */
	ret = -EINVAL;
	if (!sock->ops || !sock->ops->listen)
		goto out;

	ret = sock->ops->listen(sock);
out:
	sockfd_put(sock);
	return ret;
}

/*
 * Accept system call.
 */
int sys_accept(int sockfd, struct sockaddr *addr, size_t *addrlen)
{
	struct socket *sock, *new_sock;
	int fd, ret;

	/* unused address length */
	UNUSED(addrlen);

	/* find socket */
	sock = sockfd_lookup(sockfd, &ret);
	if (!sock)
		return ret;

	/* allocate a new socket */
	new_sock = sock_alloc();
	if (!new_sock) {
		sockfd_put(sock);
		return -ENOSR;
	}

	/* duplicate socket */
	new_sock->type = sock->type;
	new_sock->ops = sock->ops;
	ret = sock->ops->dup(sock, new_sock);
	if (ret < 0) {
		sock_release(new_sock);
		sockfd_put(sock);
		return ret;
	}

	/* accept not implemented */
	if (!new_sock->ops || !new_sock->ops->accept) {
		sock_release(new_sock);
		sockfd_put(sock);
		return -EINVAL;
	}

	/* call accept protocol */
	ret = new_sock->ops->accept(sock, new_sock, addr);
	if (ret < 0) {
		sock_release(new_sock);
		sockfd_put(sock);
		return ret;
	}

	/* get a file slot */
	fd = get_fd(new_sock->inode);
	if (fd < 0) {
		sock_release(new_sock);
		sockfd_put(sock);
		return -EINVAL;
	}

	/* set socket file */
	new_sock->file = current_task->files->filp[fd];

	sockfd_put(sock);
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
	int ret;

	/* unused address length */
	UNUSED(addrlen);

	/* find socket */
	sock = sockfd_lookup(sockfd, &ret);
	if (!sock)
		return ret;

	/* send message not implemented */
	ret = -EINVAL;
	if (!sock->ops || !sock->ops->sendmsg)
		goto out;

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
	ret = sock->ops->sendmsg(sock, &msg, sock->file->f_flags & O_NONBLOCK, flags);
out:
	sockfd_put(sock);
	return ret;
}

/*
 * Send a message system call.
 */
int sys_sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
	struct socket *sock;
	int ret;

	/* find socket */
	sock = sockfd_lookup(sockfd, &ret);
	if (!sock)
		return ret;

	/* send message not implemented */
	ret = -EINVAL;
	if (!sock->ops || !sock->ops->sendmsg)
		goto out;

	ret = sock->ops->sendmsg(sock, msg, sock->file->f_flags & O_NONBLOCK, flags);
out:
	sockfd_put(sock);
	return ret;
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
	int ret;

	/* unused address length */
	UNUSED(addrlen);

	/* find socket */
	sock = sockfd_lookup(sockfd, &ret);
	if (!sock)
		return ret;

	/* receive message not implemented */
	ret = -EINVAL;
	if (!sock->ops || !sock->ops->recvmsg)
		goto out;

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

	ret = sock->ops->recvmsg(sock, &msg, sock->file->f_flags & O_NONBLOCK, flags);
out:
	sockfd_put(sock);
	return ret;
}

/*
 * Receive a message system call.
 */
int sys_recvmsg(int sockfd, struct msghdr *msg, int flags)
{
	struct socket *sock;
	int ret;

	/* find socket */
	sock = sockfd_lookup(sockfd, &ret);
	if (!sock)
		return ret;

	/* receive message not implemented */
	ret = -EINVAL;
	if (!sock->ops || !sock->ops->recvmsg)
		goto out;

	ret = sock->ops->recvmsg(sock, msg, sock->file->f_flags & O_NONBLOCK, flags);
out:
	sockfd_put(sock);
	return ret;
}

/*
 * Shutdown system call.
 */
int sys_shutdown(int sockfd, int how)
{
	struct socket *sock;
	int ret;

	/* find socket */
	sock = sockfd_lookup(sockfd, &ret);
	if (!sock)
		return ret;

	/* shutdown not implemented */
	ret = -EINVAL;
	if (!sock->ops || !sock->ops->shutdown)
		goto out;

	ret = sock->ops->shutdown(sock, how);
out:
	sockfd_put(sock);
	return ret;
}

/*
 * Get peer name system call.
 */
int sys_getpeername(int sockfd, struct sockaddr *addr, size_t *addrlen)
{
	struct socket *sock;
	int ret;

	/* find socket */
	sock = sockfd_lookup(sockfd, &ret);
	if (!sock)
		return ret;

	/* getpeername not implemented */
	ret = -EINVAL;
	if (!sock->ops || !sock->ops->getpeername)
		goto out;

	ret = sock->ops->getpeername(sock, addr, addrlen);
out:
	sockfd_put(sock);
	return ret;
}

/*
 * Get sock name system call.
 */
int sys_getsockname(int sockfd, struct sockaddr *addr, size_t *addrlen)
{
	struct socket *sock;
	int ret;

	/* find socket */
	sock = sockfd_lookup(sockfd, &ret);
	if (!sock)
		return ret;

	/* getsockname not implemented */
	ret = -EINVAL;
	if (!sock->ops || !sock->ops->getsockname)
		goto out;

	ret = sock->ops->getsockname(sock, addr, addrlen);
out:
	sockfd_put(sock);
	return ret;
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
	int ret;

	/* find socket */
	sock = sockfd_lookup(sockfd, &ret);
	if (!sock)
		return ret;

	/* socket options */
	if (level == SOL_SOCKET) {
		ret = sock_getsockopt(sock, optname, optval, optlen);
		goto out;
	}

	/* setsockopt not implemented */
	ret = -EINVAL;
	if (!sock->ops || !sock->ops->getsockopt)
		goto out;

	ret = sock->ops->getsockopt(sock, level, optname, optval, optlen);
out:
	sockfd_put(sock);
	return ret;
}

/*
 * Set socket options system call.
 */
int sys_setsockopt(int sockfd, int level, int optname, void *optval, size_t optlen)
{
	struct socket *sock;
	int ret;

	/* find socket */
	sock = sockfd_lookup(sockfd, &ret);
	if (!sock)
		return ret;

	/* socket options */
	if (level == SOL_SOCKET) {
		ret = sock_setsockopt(sock, optname, optval, optlen);
		goto out;
	}

	/* setsockopt not implemented */
	ret = -EINVAL;
	if (!sock->ops || !sock->ops->setsockopt)
		goto out;

	ret = sock->ops->setsockopt(sock, level, optname, optval, optlen);
out:
	sockfd_put(sock);
	return ret;
}

/*
 * Socketcall system call.
 */
int sys_socketcall(int call, unsigned long *args)
{
	int ret;

	/* check call */
	if(call < 1 || call > SYS_RECVMSG)
		return -EINVAL;

	switch(call) {
		case SYS_SOCKET:
			ret = sys_socket(args[0], args[1], args[2]);
			break;
		case SYS_BIND:
			ret = sys_bind(args[0], (struct sockaddr *) args[1], args[2]);
			break;
		case SYS_CONNECT:
			ret = sys_connect(args[0], (struct sockaddr *) args[1], args[2]);
			break;
		case SYS_LISTEN:
			ret = sys_listen(args[0], args[1]);
			break;
		case SYS_ACCEPT:
			ret = sys_accept(args[0], (struct sockaddr *) args[1], (size_t *) args[2]);
			break;
		case SYS_GETSOCKNAME:
			ret = sys_getsockname(args[0],(struct sockaddr *) args[1], (size_t *) args[2]);
			break;
		case SYS_GETPEERNAME:
			ret = sys_getpeername(args[0], (struct sockaddr *) args[1], (size_t *) args[2]);
			break;
		case SYS_SEND:
			ret = sys_send(args[0], (void *) args[1], args[2], args[3]);
			break;
		case SYS_SENDTO:
			ret = sys_sendto(args[0],(void *) args[1], args[2], args[3], (struct sockaddr *) args[4], args[5]);
			break;
		case SYS_RECV:
			ret = sys_recv(args[0], (void *) args[1], args[2], args[3]);
			break;
		case SYS_RECVFROM:
			ret = sys_recvfrom(args[0], (void *) args[1], args[2], args[3], (struct sockaddr *) args[4], (size_t *) args[5]);
			break;
		case SYS_SHUTDOWN:
			ret = sys_shutdown(args[0], args[1]);
			break;
		case SYS_SETSOCKOPT:
			ret = sys_setsockopt(args[0], args[1], args[2], (char *) args[3], args[4]);
			break;
		case SYS_GETSOCKOPT:
			ret = sys_getsockopt(args[0], args[1], args[2], (char *) args[3], (size_t *) args[4]);
			break;
		case SYS_SENDMSG:
			ret = sys_sendmsg(args[0], (struct msghdr *) args[1], args[2]);
			break;
		case SYS_RECVMSG:
			ret = sys_recvmsg(args[0], (struct msghdr *) args[1], args[2]);
			break;
		default:
			ret = -EINVAL;
			break;
	}

	return ret;
}
