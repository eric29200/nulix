#include <fs/fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Truncate system call.
 */
int do_truncate(struct inode *inode, off_t length)
{
	int ret;

	/* check permissions */
	ret = permission(inode, MAY_WRITE);
	if (ret)
		return ret;

	/* set new size */
	inode->i_size = length;

	/* truncate virtual mapping */
	vmtruncate(inode, length);

	/* truncate */
	if (inode->i_op && inode->i_op->truncate)
		inode->i_op->truncate(inode);

	/* release inode */
	inode->i_dirt = 1;

	return 0;
}

/*
 * Truncate system call.
 */
int sys_truncate64(const char *pathname, off_t length)
{
	struct inode *inode;
	int ret;

	/* get inode */
	ret = namei(AT_FDCWD, NULL, pathname, 1, &inode);
	if (ret)
		return ret;

	/* truncate */
	ret = do_truncate(inode, length);

	iput(inode);
	return ret;
}

/*
 * File truncate system call.
 */
static int do_ftruncate(int fd, off_t length)
{
	struct inode *inode;
	struct file *filp;

	/* get file */
	filp = fget(fd);
	if (!filp)
		return -EBADF;

	/* get inode */
	inode = filp->f_inode;

	return do_truncate(inode, length);
}

/*
 * File truncate system call.
 */
int sys_ftruncate64(int fd, off_t length)
{
	return do_ftruncate(fd, length);
}
