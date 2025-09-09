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
	mark_inode_dirty(inode);

	return 0;
}

/*
 * Truncate system call.
 */
int sys_truncate64(const char *pathname, off_t length)
{
	struct dentry *dentry;
	int ret;

	/* resolve path */
	dentry = namei(AT_FDCWD, pathname, 1);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* truncate */
	ret = do_truncate(dentry->d_inode, length);

	dput(dentry);
	return ret;
}

/*
 * File truncate system call.
 */
static int do_ftruncate(int fd, off_t length)
{
	struct inode *inode;
	struct file *filp;
	int ret;

	/* get file */
	filp = fget(fd);
	if (!filp)
		return -EBADF;

	/* get inode */
	inode = filp->f_inode;

	/* do truncate */
	ret = do_truncate(inode, length);

	/* release file */
	fput(filp);

	return ret;
}

/*
 * File truncate system call.
 */
int sys_ftruncate64(int fd, off_t length)
{
	return do_ftruncate(fd, length);
}
