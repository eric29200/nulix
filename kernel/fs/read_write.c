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
 * Pread system call.
 */
int do_pread64(struct file_t *filp, void *buf, size_t count, off_t offset)
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
