#include <fs/fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <fcntl.h>
#include <stderr.h>
#include <string.h>

/*
 * Read system call.
 */
ssize_t do_read(struct file_t *filp, char *buf, int count)
{
	/* no data to read */
	if (!count)
		return 0;

	/* read not implemented */
	if (!filp->f_op || !filp->f_op->read)
		return -EPERM;

	return filp->f_op->read(filp, buf, count);
}

/*
 * Write system call.
 */
ssize_t do_write(struct file_t *filp, const char *buf, int count)
{
	/* no data to write */
	if (!count)
		return 0;

	/* write not implemented */
	if (!filp->f_op || !filp->f_op->write)
		return -EPERM;

	return filp->f_op->write(filp, buf, count);
}

/*
 * Generic lseek.
 */
off_t generic_lseek(struct file_t *filp, off_t offset, int whence)
{
	off_t new_offset;

	/* compute new offset */
	switch (whence) {
		case SEEK_SET:
			new_offset = offset;
			break;
		case SEEK_CUR:
			new_offset = filp->f_pos + offset;
			break;
		case SEEK_END:
			new_offset = filp->f_inode->i_size + offset;
			break;
		default:
			return -EINVAL;
	}

	/* change offset */
	filp->f_pos = new_offset;
	return filp->f_pos;
}

/*
 * Lseek system call.
 */
off_t do_lseek(struct file_t *filp, off_t offset, int whence)
{
	/* specific lseek */
	if (filp->f_op && filp->f_op->lseek)
		return filp->f_op->lseek(filp, offset, whence);

	return generic_lseek(filp, offset, whence);
}

/*
 * Lseek system call.
 */
off_t sys_lseek(int fd, off_t offset, int whence)
{
	/* check file descriptor */
	if (fd >= NR_OPEN || fd < 0 || !current_task->files->filp[fd])
		return -EBADF;

	return do_lseek(current_task->files->filp[fd], offset, whence);
}

/*
 * Llseek system call.
 */
int sys_llseek(int fd, uint32_t offset_high, uint32_t offset_low, off_t *result, int whence)
{
	off_t offset;

	/* check file descriptor */
	if (fd >= NR_OPEN || fd < 0 || !current_task->files->filp[fd])
		return -EBADF;

	/* compute offset */
	offset = ((unsigned long long) offset_high << 32) | offset_low;

	/* seek */
	*result = do_lseek(current_task->files->filp[fd], offset, whence);

	return 0;
}

/*
 * Pread system call.
 */
static int do_pread64(struct file_t *filp, void *buf, size_t count, off_t offset)
{
	off_t offset_ori;
	int ret;

	/* no data to read */
	if (!count)
		return 0;

	/* get current file */
	offset_ori = filp->f_pos;

	/* read not implemented */
	if (!filp->f_op || !filp->f_op->read)
		return -EPERM;

	/* seek to offset */
	ret = do_lseek(filp, offset, SEEK_SET);
	if (ret < 0)
		return ret;

	/* read data */
	ret = filp->f_op->read(filp, buf, count);

	/* restore initial position */
	filp->f_pos = offset_ori;

	return ret;
}

/*
 * Read system call.
 */
int sys_read(int fd, char *buf, int count)
{
	/* check file descriptor */
	if (fd >= NR_OPEN || fd < 0 || count < 0 || !current_task->files->filp[fd])
		return -EBADF;

	return do_read(current_task->files->filp[fd], buf, count);
}

/*
 * Write system call.
 */
int sys_write(int fd, const char *buf, int count)
{
	/* check file descriptor */
	if (fd >= NR_OPEN || fd < 0 || count < 0 || !current_task->files->filp[fd])
		return -EBADF;

	return do_write(current_task->files->filp[fd], buf, count);
}

/*
 * Read data into multiple buffers.
 */
ssize_t sys_readv(int fd, const struct iovec_t *iov, int iovcnt)
{
	ssize_t ret = 0, n;
	int i;

	/* check file descriptor */
	if (fd >= NR_OPEN || fd < 0 || !current_task->files->filp[fd])
		return -EBADF;

	/* read into each buffer */
	for (i = 0; i < iovcnt; i++, iov++) {
		/* read into buffer */
		n = do_read(current_task->files->filp[fd], iov->iov_base, iov->iov_len);
		if (n < 0)
			return n;

		/* check end of file */
		ret += n;
		if (n != (ssize_t) iov->iov_len)
			break;
	}

	return ret;
}

/*
 * Write data from multiple buffers.
 */
ssize_t sys_writev(int fd, const struct iovec_t *iov, int iovcnt)
{
	ssize_t ret = 0, n;
	int i;

	/* check file descriptor */
	if (fd >= NR_OPEN || fd < 0 || !current_task->files->filp[fd])
		return -EBADF;

	/* write each buffer */
	for (i = 0; i < iovcnt; i++, iov++) {
		/* write into buffer */
		n = do_write(current_task->files->filp[fd], iov->iov_base, iov->iov_len);
		if (n < 0)
			return n;

		/* check end of file */
		ret += n;
		if (n != (ssize_t) iov->iov_len)
			break;
	}

	return ret;
}

/*
 * Pread64 system call.
 */
int sys_pread64(int fd, void *buf, size_t count, off_t offset)
{
	/* check file descriptor */
	if (fd >= NR_OPEN || fd < 0 || !current_task->files->filp[fd])
		return -EBADF;

	return do_pread64(current_task->files->filp[fd], buf, count, offset);
}

/*
 * Copy file range system call.
 */
int sys_copy_file_range(int fd_in, off_t *off_in, int fd_out, off_t *off_out, size_t len, unsigned int flags)
{
	UNUSED(fd_in);
	UNUSED(off_in);
	UNUSED(fd_out);
	UNUSED(off_out);
	UNUSED(len);
	UNUSED(flags);
	return 0;
}