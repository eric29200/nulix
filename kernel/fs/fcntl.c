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
	current_task->files->filp[newfd] = current_task->files->filp[oldfd];
	current_task->files->filp[newfd]->f_count++;

	return newfd;
}

/*
 * Dup2 system call.
 */
int sys_dup2(int oldfd, int newfd)
{
	int ret;

	/* check parameters */
	if (oldfd < 0 || oldfd >= NR_OPEN || !current_task->files->filp[oldfd] || newfd < 0 || newfd >= NR_OPEN)
		return -EBADF;

	/* same fd */
	if (oldfd == newfd)
		return oldfd;

	/* close existing file */
	if (current_task->files->filp[newfd] != NULL) {
		ret = sys_close(newfd);
		if (ret < 0)
			return ret;
	}

	/* duplicate */
	return dupfd(oldfd, newfd);
}

/*
 * Dup system call.
 */
int sys_dup(int oldfd)
{
	int newfd;

	/* check parameter */
	if (oldfd < 0 || oldfd >= NR_OPEN || current_task->files->filp[oldfd] == NULL)
		return -EBADF;

	/* find a free slot */
	for (newfd = 0; newfd < NR_OPEN; newfd++)
		if (current_task->files->filp[newfd] == NULL)
			return dupfd(oldfd, newfd);

	/* no free slot : too many files open */
	return -EMFILE;
}

/*
 * Dup system call.
 */
static int dup_after(int oldfd, int min_slot)
{
	int newfd;

	/* find a free slot */
	for (newfd = min_slot; newfd < NR_OPEN; newfd++)
		if (current_task->files->filp[newfd] == NULL)
			return dupfd(oldfd, newfd);

	/* no free slot : too many files open */
	return -EMFILE;
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
		return -EINVAL;

	switch (cmd) {
		case F_DUPFD:
			ret = dup_after(fd, arg);
			break;
		case F_DUPFD_CLOEXEC:
			ret = dup_after(fd, arg);
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
		default:
			printf("unknown fcntl command %d\n", cmd);
			break;
	}

	return ret;
}
