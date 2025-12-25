#include <fs/fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stdio.h>
#include <fcntl.h>
#include <stderr.h>
#include <string.h>

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
static ssize_t do_read(struct file *filp, char *buf, size_t count, off_t *ppos)
{
	ssize_t ret;

	/* no data to read */
	if (!count)
		return 0;

	/* read not implemented */
	if (!filp->f_op || !filp->f_op->read)
		return -EPERM;

	/* read */
	ret = filp->f_op->read(filp, buf, count, ppos);

	/* update io accounting */
	if (ret > 0)
		current_task->ioac.rchar += ret;

	return ret;
}

/*
 * Write system call.
 */
static ssize_t do_write(struct file *filp, const char *buf, size_t count, off_t *ppos)
{
	ssize_t ret;

	/* no data to write */
	if (!count)
		return 0;

	/* write not implemented */
	if (!filp->f_op || !filp->f_op->write)
		return -EPERM;

	/* write */
	ret = filp->f_op->write(filp, buf, count, ppos);

	/* update io accounting */
	if (ret > 0)
		current_task->ioac.wchar += ret;

	return ret;
}

/*
 * Read/write vectors.
 */
static ssize_t do_readv_writev(int type, struct file *filp, const struct iovec *iovec, size_t iovcnt)
{
	typedef ssize_t (*io_fn_t)(struct file *, char *, size_t, off_t *);
	struct inode *inode = filp->f_dentry->d_inode;
	struct iovec iovstack[UIO_FASTIOV];
	struct iovec *iov = iovstack;
	ssize_t len, nr;
	void * base;
	io_fn_t fn;
	int ret;


	/* check count */
	if (!iovcnt)
		return 0;
	if (iovcnt > UIO_MAXIOV)
		return -EINVAL;

	/* allocate kernel vectors if needed */
	if (iovcnt > UIO_FASTIOV) {
		iov = kmalloc(sizeof(struct iovec) * iovcnt);
		if (!iov)
			return -ENOMEM;
	}

	/* copy vectors from user space */
	memcpy(iov, iovec, sizeof(struct iovec) * iovcnt);

	/* socket read/write */
	if (inode->i_sock) {
		ret = sock_readv_writev(type, filp, iov, iovcnt);
		goto out;
	}

	/* check file operations */
	ret = -EINVAL;
	if (!filp->f_op)
		goto out;

	/* get file operations (read or write) */
	fn = filp->f_op->read;
	if (type == WRITE)
		fn = (io_fn_t) filp->f_op->write;

	/* read/write */
	ret = 0;
	for (iovec = iov; iovcnt > 0; iovec++, iovcnt--) {
		/* get vector */
		base = iovec->iov_base;
		len = iovec->iov_len;

		/* read/write */
		nr = fn(filp, base, len, &filp->f_pos);
		if (nr < 0) {
			if (!ret)
				ret = nr;
			break;
		}

		/* update numbers of characters read/written */
		ret += nr;

		/* partial read/write : exit */
		if (nr != len)
			break;
	}

out:
	if (iov != iovstack)
		kfree(iov);
	return ret;
}

/*
 * Read system call.
 */
int sys_read(int fd, char *buf, int count)
{
	struct file *filp;
	int ret = -EBADF;

	/* get file */
	filp = fget(fd);
	if (!filp)
		goto out;

	/* do read */
	ret = do_read(filp, buf, count, &filp->f_pos);

	/* release file */
	fput(filp);
out:
	current_task->ioac.syscr++;
	return ret;
}

/*
 * Write system call.
 */
int sys_write(int fd, const char *buf, int count)
{
	struct file *filp;
	int ret = -EBADF;

	/* get file */
	filp = fget(fd);
	if (!filp)
		goto out;

	/* do write */
	ret = do_write(filp, buf, count, &filp->f_pos);

	/* release file */
	fput(filp);
out:
	current_task->ioac.syscw++;
	return ret;
}

/*
 * Read data into multiple buffers.
 */
ssize_t sys_readv(int fd, const struct iovec *iov, int iovcnt)
{
	ssize_t ret = -EBADF;
	struct file *filp;

	/* get file */
	filp = fget(fd);
	if (!filp)
		goto out;

	/* read */
	if (filp->f_op && filp->f_op->read && (filp->f_mode & FMODE_READ))
		ret = do_readv_writev(READ, filp, iov, iovcnt);

	/* release file */
	fput(filp);
out:
	if (ret > 0)
		current_task->ioac.rchar += ret;
	current_task->ioac.syscr++;
	return ret;
}

/*
 * Write data from multiple buffers.
 */
ssize_t sys_writev(int fd, const struct iovec *iov, int iovcnt)
{
	ssize_t ret = -EBADF;
	struct file *filp;

	/* get file */
	filp = fget(fd);
	if (!filp)
		goto out;

	/* write */
	if (filp->f_op && filp->f_op->write && (filp->f_mode & FMODE_WRITE))
		ret = do_readv_writev(WRITE, filp, iov, iovcnt);

	/* release file */
	fput(filp);
out:
	if (ret > 0)
		current_task->ioac.wchar += ret;
	current_task->ioac.syscw++;
	return ret;
}

/*
 * Pread64 system call.
 */
int sys_pread64(int fd, void *buf, size_t count, off_t offset)
{
	struct file *filp;
	int ret = EBADF;

	/* get file */
	filp = fget(fd);
	if (!filp)
		goto out;

	/* do pread */
	ret = do_read(filp, buf, count, &offset);

	/* release file */
	fput(filp);
out:
	current_task->ioac.syscr++;
	return ret;
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

	/* update io accounting */
	current_task->ioac.syscr++;
	current_task->ioac.syscw++;

	return n < 0 ? n : tot;
}
