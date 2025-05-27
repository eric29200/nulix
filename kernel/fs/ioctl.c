#include <fs/fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Ioctl system call.
 */
int sys_ioctl(int fd, unsigned long request, unsigned long arg)
{
	struct file *filp;
	int on;

	/* get file */
	filp = fget(fd);
	if (!filp)
		return -EBADF;

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
