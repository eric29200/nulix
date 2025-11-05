#include <fs/fs.h>
#include <proc/sched.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Duplicate a file.
 */
static int dupfd(int oldfd, int newfd)
{
	/* check old file descriptor */
	if (oldfd < 0 || oldfd >= NR_OPEN || !current_task->files->filp[oldfd])
		return -EBADF;

	/* check new file descriptor */
	if (newfd < 0 || newfd >= NR_OPEN)
		return -EINVAL;

	/* find a free slot */
	for (;newfd < NR_OPEN; newfd++)
		if (current_task->files->filp[newfd] == NULL)
			break;

	/* no free slot */
	if (newfd >= NR_OPEN)
		return -EMFILE;

	/* install new file */
	FD_CLR(oldfd, &current_task->files->close_on_exec);
	current_task->files->filp[newfd] = current_task->files->filp[oldfd];
	current_task->files->filp[newfd]->f_count++;

	return newfd;
}

/*
 * Dup2 system call.
 */
int sys_dup2(int oldfd, int newfd)
{
	/* check old file descriptor */
	if (oldfd < 0 || oldfd >= NR_OPEN || !current_task->files->filp[oldfd])
		return -EBADF;

	/* check new file descriptor */
	if (newfd < 0 || newfd >= NR_OPEN)
		return -EBADF;

	/* same fd */
	if (oldfd == newfd)
		return oldfd;

	/* duplicate */
	sys_close(newfd);
	return dupfd(oldfd, newfd);
}

/*
 * Dup system call.
 */
int sys_dup(int oldfd)
{
	return dupfd(oldfd, 0);
}

/*
 * Fcntl system call.
 */
int sys_fcntl(int fd, int cmd, unsigned long arg)
{
	struct file *filp;
	int ret = 0;

	/* get file */
	filp = fget(fd);
	if (!filp)
		return -EBADF;

	switch (cmd) {
		case F_DUPFD:
			ret = dupfd(fd, arg);
			break;
		case F_DUPFD_CLOEXEC:
			ret = dupfd(fd, arg);
			if (ret >= 0)
				FD_SET(ret, &current_task->files->close_on_exec);
			break;
		case F_GETFD:
			ret = FD_ISSET(fd, &current_task->files->close_on_exec);
			break;
		case F_SETFD:
			if (arg & 1)
				FD_SET(fd, &current_task->files->close_on_exec);
			else
				FD_CLR(fd, &current_task->files->close_on_exec);
			break;
		case F_GETFL:
			ret = filp->f_flags;
			break;
		case F_SETFL:
			filp->f_flags = arg;
			break;
		case F_GETOWN:
			ret = filp->f_owner.pid;
			break;
		case F_SETOWN:
			filp->f_owner.pid = arg;
			filp->f_owner.uid = current_task->uid;
			filp->f_owner.euid = current_task->euid;
			if (S_ISSOCK(filp->f_dentry->d_inode->i_mode))
				ret = sock_fcntl(filp, F_SETOWN, arg);
			break;
		default:
			printf("unknown fcntl command %d\n", cmd);
			break;
	}

	/* release file */
	fput(filp);

	return ret;
}
