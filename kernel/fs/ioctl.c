#include <fs/fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Ioctl system call.
 */
int do_ioctl(int fd, int request, unsigned long arg)
{
	struct file_t *filp;
	int on;

	/* check input args */
	if (fd >= NR_OPEN || fd < 0 || !current_task->files->filp[fd])
		return -EBADF;

	/* get current file */
	filp = current_task->files->filp[fd];

	switch (request) {
		case FIONBIO:
			on = *((int *) arg);

			if (on)
				filp->f_flags |= O_NONBLOCK;
			else
				filp->f_flags &= ~O_NONBLOCK;

			return 0;
		case FIONREAD:
			*((int *) arg) = filp->f_inode->i_size - filp->f_pos;
			return 0;
		default:
			/* ioctl not implemented */
			if (!filp->f_op || !filp->f_op->ioctl)
				return -EPERM;

			return filp->f_op->ioctl(filp, request, arg);
	}
}
