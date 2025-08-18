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
ssize_t do_read(struct file *filp, char *buf, int count)
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
ssize_t do_write(struct file *filp, const char *buf, int count)
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
off_t generic_file_llseek(struct file *filp, off_t offset, int whence)
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
off_t do_llseek(struct file *filp, off_t offset, int whence)
{
	/* specific llseek */
	if (filp->f_op && filp->f_op->llseek)
		return filp->f_op->llseek(filp, offset, whence);

	return generic_file_llseek(filp, offset, whence);
}

/*
 * Lseek system call.
 */
off_t sys_lseek(int fd, off_t offset, int whence)
{
	struct file *filp;

	/* get file */
	filp = fget(fd);
	if (!filp)
		return -EBADF;

	return do_llseek(filp, offset, whence);
}

/*
 * Llseek system call.
 */
int sys_llseek(int fd, uint32_t offset_high, uint32_t offset_low, off_t *result, int whence)
{
	struct file *filp;
	off_t offset;

	/* get file */
	filp = fget(fd);
	if (!filp)
		return -EBADF;

	/* compute offset */
	offset = ((unsigned long long) offset_high << 32) | offset_low;

	/* seek */
	*result = do_llseek(filp, offset, whence);

	return 0;
}

/*
 * Pread system call.
 */
static int do_pread64(struct file *filp, void *buf, size_t count, off_t offset)
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
	ret = do_llseek(filp, offset, SEEK_SET);
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
	struct file *filp;

	/* get file */
	filp = fget(fd);
	if (!filp)
		return -EBADF;

	return do_read(filp, buf, count);
}

/*
 * Write system call.
 */
int sys_write(int fd, const char *buf, int count)
{
	struct file *filp;

	/* get file */
	filp = fget(fd);
	if (!filp)
		return -EBADF;

	return do_write(filp, buf, count);
}

/*
 * Read data into multiple buffers.
 */
ssize_t sys_readv(int fd, const struct iovec *iov, int iovcnt)
{
	ssize_t ret = 0, n;
	struct file *filp;
	int i;

	/* get file */
	filp = fget(fd);
	if (!filp)
		return -EBADF;

	/* read into each buffer */
	for (i = 0; i < iovcnt; i++, iov++) {
		/* read into buffer */
		n = do_read(filp, iov->iov_base, iov->iov_len);
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
ssize_t sys_writev(int fd, const struct iovec *iov, int iovcnt)
{
	ssize_t ret = 0, n;
	struct file *filp;
	int i;

	/* get file */
	filp = fget(fd);
	if (!filp)
		return -EBADF;

	/* write each buffer */
	for (i = 0; i < iovcnt; i++, iov++) {
		/* write into buffer */
		n = do_write(filp, iov->iov_base, iov->iov_len);
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
	struct file *filp;

	/* get file */
	filp = fget(fd);
	if (!filp)
		return -EBADF;

	return do_pread64(filp, buf, count, offset);
}

/*
 * Copy file range system call.
 */
int sys_copy_file_range(int fd_in, off_t *off_in, int fd_out, off_t *off_out, size_t len, unsigned int flags)
{
	printf("copy_file_range() unimplemented\n");
	UNUSED(fd_in);
	UNUSED(off_in);
	UNUSED(fd_out);
	UNUSED(off_out);
	UNUSED(len);
	UNUSED(flags);
	return 0;
}

/*
 * Send file system call.
 */
ssize_t sys_sendfile64(int fd_out, int fd_in, off_t *offset, size_t count)
{
	struct file *filp_in, *filp_out;
	ssize_t n, tot = 0;
	size_t f_pos;
	int buf_len;
	void *buf;

	/* get input file */
	filp_in = fget(fd_in);
	if (!filp_in)
		return -EBADF;

	/* get output file */
	filp_out = fget(fd_out);
	if (!filp_out)
		return -EBADF;

	/* get a free buffer */
	buf = get_free_page();
	if (!buf)
		return -ENOMEM;

	/* set input file position */
	if (offset) {
		f_pos = filp_in->f_pos;
		filp_in->f_pos = *offset;
	}

	/* send */
	while (count > 0) {
		buf_len = PAGE_SIZE < count ? PAGE_SIZE : count;

		/* read from input file */
		n = do_read(filp_in, buf, buf_len);
		if (n <= 0)
			break;

		/* write to output file */
		n = do_write(filp_out, buf, n);
		if (n <= 0)
			break;

		count -= n;
		tot += n;
	}

	/* update offset */
	if (offset) {
		*offset = filp_in->f_pos;
		filp_in->f_pos = f_pos;
	}

	/* free page */
	free_page(buf);

	return n < 0 ? n : tot;
}
