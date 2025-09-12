#include <fs/fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * File ioctl.
 */
static int file_ioctl(struct file *filp, unsigned long request, unsigned long arg)
{
	struct inode *inode = filp->f_dentry->d_inode;
	int ret = 0;

	switch (request) {
		case FIONREAD:
			*((int *) arg) = inode->i_size - filp->f_pos;
			break;
		default:
			/* ioctl not implemented */
			ret = -EPERM;
			if (!filp->f_op || !filp->f_op->ioctl)
				break;

			ret = filp->f_op->ioctl(filp, request, arg);
			break;
	}

	return ret;
}

/*
 * Ioctl system call.
 */
int sys_ioctl(int fd, unsigned long request, unsigned long arg)
{
	struct file *filp;
	int ret = 0, on;

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

			break;
		default:
		 	/* check inode */
			ret = -ENOENT;
			if (!filp->f_dentry || !filp->f_dentry->d_inode)
				break;

			ret = file_ioctl(filp, request, arg);
			break;
	}

	/* release file */
	fput(filp);

	return ret;
}
