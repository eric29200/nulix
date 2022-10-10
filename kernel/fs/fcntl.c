#include <fs/fs.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Duplicate a file.
 */
static int dupfd(int oldfd, int newfd)
{
	current_task->files->filp[newfd] = current_task->files->filp[oldfd];
	current_task->files->filp[newfd]->f_ref++;

	return newfd;
}

/*
 * Dup2 system call.
 */
int do_dup2(int oldfd, int newfd)
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
		ret = do_close(newfd);
		if (ret < 0)
			return ret;
	}

	/* duplicate */
	return dupfd(oldfd, newfd);
}

/*
 * Dup system call.
 */
int do_dup(int oldfd)
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
int do_fcntl(int fd, int cmd, unsigned long arg)
{
	struct file_t *filp;
	int ret = 0;

	/* check fd */
	if (fd >= NR_OPEN || !current_task->files->filp[fd])
		return -EINVAL;

	filp = current_task->files->filp[fd];
	switch (cmd) {
		case F_DUPFD:
		case F_DUPFD_CLOEXEC:
			ret = dup_after(fd, arg);
			break;
		case F_GETFD:
			ret = filp->f_mode;
			break;
		case F_SETFD:
			filp->f_mode = arg;
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
