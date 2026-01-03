#include <fs/fs.h>
#include <net/sock.h>
#include <proc/sched.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>
#include <fcntl.h>
#include <uio.h>

#define MAX_SOCK_ADDR		128

/* global variables */
struct net_proto_family *net_families[NPROTO];
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

	return sock;
}

/*
 * Release a socket.
 */
static void sock_release(struct socket *sock)
{
	/* set state */
	if (sock->state != SS_UNCONNECTED)
		sock->state = SS_DISCONNECTING;

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
	inode->i_count++;

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
 * Send a message.
 */
static int sock_sendmsg(struct socket *sock, struct msghdr *msg, size_t len)
{
	/* send message not implemented */
	if (!sock->ops || !sock->ops->sendmsg)
		return -EINVAL;

	/* send message */
	return sock->ops->sendmsg(sock, msg, len);
}

/*
 * Receive a message.
 */
static int sock_recvmsg(struct socket *sock, struct msghdr *msg, size_t len)
{
	/* receive message not implemented */
	if (!sock->ops || !sock->ops->recvmsg)
		return -EINVAL;

	/* send message */
	return sock->ops->recvmsg(sock, msg, len);
}
/*
 * Close a socket.
 */
static int sock_close(struct inode *inode, struct file *filp)
{
	UNUSED(filp);

	/* check inode */
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
static int sock_ioctl(struct inode *inode, struct file *filp, int cmd, unsigned long arg)
{
	struct socket *sock;

	UNUSED(filp);

	/* get socket */
	sock = &inode->u.socket_i;
	if (!sock)
		return -EINVAL;

	return sock->ops->ioctl(sock, cmd, arg);
}

/*
 * Socket read.
 */
static int sock_read(struct file *filp, char *buf, size_t len, off_t *ppos)
{
	struct socket *sock;
	struct msghdr msg;
	struct iovec iov;

	/* check position */
	if (ppos != &filp->f_pos)
		return -ESPIPE;

	/* get socket */
	sock = &filp->f_dentry->d_inode->u.socket_i;
	if (!sock)
		return -EINVAL;

	/* build message */
	memset(&msg, 0, sizeof(struct msghdr));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_flags = filp->f_flags & O_NONBLOCK ? MSG_DONTWAIT : 0;
	iov.iov_base = buf;
	iov.iov_len = len;

	return sock_recvmsg(sock, &msg, len);
}

/*
 * Socket write.
 */
static int sock_write(struct file *filp, const char *buf, size_t len, off_t *ppos)
{
	struct socket *sock;
	struct msghdr msg;
	struct iovec iov;

	/* check position */
	if (ppos != &filp->f_pos)
		return -ESPIPE;

	/* get socket */
	sock = &filp->f_dentry->d_inode->u.socket_i;
	if (!sock)
		return -EINVAL;

	/* build message */
	memset(&msg, 0, sizeof(struct msghdr));
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	msg.msg_flags = filp->f_flags & O_NONBLOCK ? MSG_DONTWAIT : 0;
	iov.iov_base = (char *) buf;
	iov.iov_len = len;

	return sock_sendmsg(sock, &msg, len);
}

/*
 * Read/write vectors.
 */
int sock_readv_writev(int type, struct file *filp, const struct iovec *iov, int iovcnt, size_t len)
{
	struct socket *sock;
	struct msghdr msg;

	/* get socket */
	sock = &filp->f_dentry->d_inode->u.socket_i;
	if (!sock)
		return -EINVAL;

	/* build message */
	memset(&msg, 0, sizeof(struct msghdr));
	msg.msg_iov = (struct iovec *) iov;
	msg.msg_iovlen = iovcnt;
	msg.msg_flags = filp->f_flags & O_NONBLOCK ? MSG_DONTWAIT : 0;

	/* write or read */
	if (type == WRITE)
		return sock_sendmsg(sock, &msg, len);
	return sock_recvmsg(sock, &msg, len);
}

/*
 * Socket fcntl.
 */
int sock_fcntl(struct file *filp, int cmd, unsigned long arg)
{
	struct socket *sock;

	/* get socket */
	sock = &filp->f_dentry->d_inode->u.socket_i;
	if (!sock)
		return -EINVAL;

	switch (cmd) {
		case F_SETOWN:
			if (current_task->pgrp != (pid_t) -arg && current_task->pid != (pid_t) arg)
				return -EPERM;
			sock->sk->proc = arg;
			return 0;
		case F_GETOWN:
			return sock->sk->proc;
		default:
			return -EINVAL;
	}
}

/*
 * Socket file operations.
 */
struct file_operations socket_fops = {
	.read		= sock_read,
	.write		= sock_write,
	.poll		= sock_poll,
	.ioctl		= sock_ioctl,
	.release	= sock_close,
};

/*
 * Create a socket.
 */
static int sock_create(int family, int type, int protocol, struct socket **res)
{
	struct socket *sock;
	int ret;

	/* check protocol */
	if (family < 0 || family >= NPROTO || !net_families[family])
		return -EAFNOSUPPORT;

	/* check type */
	type &= SOCK_TYPE_MASK;
	if ((type != SOCK_STREAM && type != SOCK_DGRAM && type != SOCK_RAW) || protocol < 0)
		return -EINVAL;

	/* allocate a socket */
	sock = sock_alloc();
	if (!sock)
		return -ENFILE;

	/* set socket */
	sock->family = family;
	sock->type = type;

	/* create socket */
	ret = net_families[family]->create(sock, protocol);
	if (ret) {
		sock_release(sock);
		return ret;
	}

	*res = sock;
	return 0;
}

/*
 * Create a socket.
 */
int sys_socket(int domain, int type, int protocol)
{
	struct socket *sock;
	int ret, fd;

	/* create socket */
	ret = sock_create(domain, type, protocol, &sock);
	if (ret)
		return ret;

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
	char address[MAX_SOCK_ADDR];
	size_t len;
	int ret;

	/* unused address length */
	UNUSED(addrlen);

	/* find socket */
	sock = sockfd_lookup(sockfd, &ret);
	if (!sock)
		goto out;

restart:
	/* allocate a new socket */
	ret = -EMFILE;
	new_sock = sock_alloc();
	if (!new_sock)
		goto out_put;

	/* duplicate socket */
	new_sock->type = sock->type;
	new_sock->ops = sock->ops;
	ret = sock->ops->dup(new_sock, sock);
	if (ret < 0)
		goto out_release;

	/* accept not implemented */
	ret = -EINVAL;
	if (!new_sock->ops || !new_sock->ops->accept)
		goto out_release;

	/* call accept protocol */
	ret = new_sock->ops->accept(sock, new_sock, sock->file->f_flags);
	if (ret < 0)
		goto out_release;

	/* get a file slot */
	ret = get_fd(new_sock->inode);
	if (ret < 0)
		goto out_release;

	/* set socket file */
	new_sock->file = current_task->files->filp[ret];

	/* get address */
	if (addr) {
		if (new_sock->ops->getname(new_sock, (struct sockaddr *) address, &len, 1) < 0) {
			sys_close(ret);
			goto restart;
		}

		memcpy(addr, address, len < *addrlen ? len : *addrlen);
		*addrlen = len;
	}
out_put:
	sockfd_put(sock);
out:
	return ret;
out_release:
	sock_release(sock);
	goto out_put;
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
	msg.msg_flags = flags;
	if (sock->file->f_flags & O_NONBLOCK)
		msg.msg_flags |= MSG_DONTWAIT;

	/* send message */
	ret = sock_sendmsg(sock, &msg, len);

	sockfd_put(sock);
	return ret;
}

/*
 * Send a message system call.
 */
int sys_sendmsg(int sockfd, const struct msghdr *msg, int flags)
{
	struct iovec iovstack[UIO_FASTIOV];
	struct iovec *iov = iovstack;
	size_t total_len = 0, i;
	struct msghdr msg_sys;
	struct socket *sock;
	int ret;

	/* find socket */
	sock = sockfd_lookup(sockfd, &ret);
	if (!sock)
		return ret;

	/* copy message to kernel space */
	memcpy(&msg_sys, msg, sizeof(struct msghdr));
	ret = -EINVAL;
	if (msg_sys.msg_iovlen > UIO_MAXIOV)
		goto out;

	/* allocate vectors if needed */
	ret = -ENOMEM;
	if (msg_sys.msg_iovlen > UIO_FASTIOV) {
		iov = (struct iovec *) kmalloc(msg_sys.msg_iovlen * sizeof(struct iovec));
		if (!iov)
			goto out;
	}

	/* copy vectors from user to kernel space */
	memcpy(iov, msg_sys.msg_iov, sizeof(struct iovec) * msg_sys.msg_iovlen);

	/* compute total length */
	for (i = 0; i < msg_sys.msg_iovlen; i++)
		total_len += msg_sys.msg_iov[i].iov_len;

	/* set flags */
	msg_sys.msg_flags = flags;
	if (sock->file->f_flags & O_NONBLOCK)
		msg_sys.msg_flags |= MSG_DONTWAIT;

	/* send message */
	ret = sock_sendmsg(sock, &msg_sys, total_len);

	/* free vectors */
	if (iov != iovstack)
		kfree(iov);
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
	msg.msg_flags = flags;
	if (sock->file->f_flags & O_NONBLOCK)
		msg.msg_flags |= MSG_DONTWAIT;

	ret = sock_recvmsg(sock, &msg, len);

	sockfd_put(sock);
	return ret;
}

/*
 * Receive a message system call.
 */
int sys_recvmsg(int sockfd, struct msghdr *msg, int flags)
{
	struct iovec iovstack[UIO_FASTIOV];
	struct iovec *iov = iovstack;
	size_t total_len = 0, i;
	struct msghdr msg_sys;
	struct socket *sock;
	int ret;

	/* find socket */
	sock = sockfd_lookup(sockfd, &ret);
	if (!sock)
		return ret;

	/* copy message to kernel space */
	memcpy(&msg_sys, msg, sizeof(struct msghdr));
	ret = -EINVAL;
	if (msg_sys.msg_iovlen > UIO_MAXIOV)
		goto out;

	/* allocate vectors if needed */
	ret = -ENOMEM;
	if (msg_sys.msg_iovlen > UIO_FASTIOV) {
		iov = (struct iovec *) kmalloc(msg_sys.msg_iovlen * sizeof(struct iovec));
		if (!iov)
			goto out;
	}

	/* copy vectors from user to kernel space */
	memcpy(iov, msg_sys.msg_iov, sizeof(struct iovec) * msg_sys.msg_iovlen);

	/* compute total length */
	for (i = 0; i < msg_sys.msg_iovlen; i++)
		total_len += msg_sys.msg_iov[i].iov_len;

	/* set flags */
	msg_sys.msg_flags = flags;
	if (sock->file->f_flags & O_NONBLOCK)
		msg_sys.msg_flags |= MSG_DONTWAIT;

	/* receive message */
	ret = sock_recvmsg(sock, &msg_sys, total_len);

	/* free vectors */
	if (iov != iovstack)
		kfree(iov);
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
	if (!sock->ops || !sock->ops->getname)
		goto out;

	ret = sock->ops->getname(sock, addr, addrlen, 1);
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
	if (!sock->ops || !sock->ops->getname)
		goto out;

	ret = sock->ops->getname(sock, addr, addrlen, 0);
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
	if (call < 1 || call > SYS_RECVMSG)
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

/*
 * Register a procotol family.
 */
int sock_register(struct net_proto_family *ops)
{
	/* check faimily */
	if (ops->family >= NPROTO) {
		printf("sock_register: protocol %d >= NPROTO(%d)\n", ops->family, NPROTO);
		return -ENOBUFS;
	}

	net_families[ops->family] = ops;
	return 0;
}

/*
 * Init protocols.
 */
void init_proto()
{
	unix_proto_init();
	inet_proto_init();
}