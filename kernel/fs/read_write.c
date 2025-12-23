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
static ssize_t do_read(struct file *filp, char *buf, size_t count, off_t *ppos)
{
	/* no data to read */
	if (!count)
		return 0;

	/* read not implemented */
	if (!filp->f_op || !filp->f_op->read)
		return -EPERM;

	return filp->f_op->read(filp, buf, count, ppos);
}

/*
 * Write system call.
 */
static ssize_t do_write(struct file *filp, const char *buf, size_t count, off_t *ppos)
{
	/* no data to write */
	if (!count)
		return 0;

	/* write not implemented */
	if (!filp->f_op || !filp->f_op->write)
		return -EPERM;

	return filp->f_op->write(filp, buf, count, ppos);
}

/*
 * Generic lseek.
 */
static off_t generic_file_llseek(struct file *filp, off_t offset, int whence)
{
	struct dentry *dentry;
	struct inode *inode;
	off_t new_offset;

	/* get dentry */
	dentry = filp->f_dentry;
	if (!dentry)
		return -ENOENT;

	/* get inode */
	inode = dentry->d_inode;
	if (!inode)
		return -ENOENT;

	/* compute new offset */
	switch (whence) {
		case SEEK_SET:
			new_offset = offset;
			break;
		case SEEK_CUR:
			new_offset = filp->f_pos + offset;
			break;
		case SEEK_END:
			new_offset = inode->i_size + offset;
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
static off_t do_llseek(struct file *filp, off_t offset, int whence)
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
	int ret;

	/* get file */
	filp = fget(fd);
	if (!filp)
		return -EBADF;

	/* do llseek */
	ret = do_llseek(filp, offset, whence);

	/* release file */
	fput(filp);

	return ret;
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

	/* release file */
	fput(filp);

	return 0;
}

/*
 * Read system call.
 */
int sys_read(int fd, char *buf, int count)
{
	struct file *filp;
	int ret;

	/* get file */
	filp = fget(fd);
	if (!filp)
		return -EBADF;

	/* do read */
	ret = do_read(filp, buf, count, &filp->f_pos);

	/* release file */
	fput(filp);

	return ret;
}

/*
 * Write system call.
 */
int sys_write(int fd, const char *buf, int count)
{
	struct file *filp;
	int ret;

	/* get file */
	filp = fget(fd);
	if (!filp)
		return -EBADF;

	/* do write */
	ret = do_write(filp, buf, count, &filp->f_pos);

	/* release file */
	fput(filp);

	return ret;
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
		n = do_read(filp, iov->iov_base, iov->iov_len, &filp->f_pos);
		if (n < 0) {
			ret = n;
			break;
		}

		/* check end of file */
		ret += n;
		if (n != (ssize_t) iov->iov_len)
			break;
	}

	/* release file */
	fput(filp);

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
		n = do_write(filp, iov->iov_base, iov->iov_len, &filp->f_pos);
		if (n < 0) {
			ret = n;
			break;
		}

		/* check end of file */
		ret += n;
		if (n != (ssize_t) iov->iov_len)
			break;
	}

	/* release file */
	fput(filp);

	return ret;
}

/*
 * Pread64 system call.
 */
int sys_pread64(int fd, void *buf, size_t count, off_t offset)
{
	struct file *filp;
	int ret;

	/* get file */
	filp = fget(fd);
	if (!filp)
		return -EBADF;

	/* do pread */
	ret = do_read(filp, buf, count, &offset);

	/* release file */
	fput(filp);

	return ret;
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
	off_t *f_pos;
	int buf_len;
	void *buf;

	/* get input file */
	filp_in = fget(fd_in);
	if (!filp_in)
		return -EBADF;

	/* get output file */
	filp_out = fget(fd_out);
	if (!filp_out) {
		fput(filp_in);
		return -EBADF;
	}

	/* get a free buffer */
	buf = get_free_page();
	if (!buf) {
		fput(filp_in);
		fput(filp_out);
		return -ENOMEM;
	}

	/* get input file position */
	f_pos = offset ? offset : &filp_in->f_pos;

	/* send */
	while (count > 0) {
		buf_len = PAGE_SIZE < count ? PAGE_SIZE : count;

		/* read from input file */
		n = do_read(filp_in, buf, buf_len, f_pos);
		if (n <= 0)
			break;

		/* write to output file */
		n = do_write(filp_out, buf, n, &filp_out->f_pos);
		if (n <= 0)
			break;

		count -= n;
		tot += n;
	}

	/* release files */
	fput(filp_in);
	fput(filp_out);

	/* free page */
	free_page(buf);

	return n < 0 ? n : tot;
}
