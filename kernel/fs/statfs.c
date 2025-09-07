#include <fs/fs.h>
#include <proc/sched.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Get statistics on file system.
 */
static int do_statfs64(struct inode *inode, struct statfs64 *buf)
{
	/* check if statfs is implemented */
	if (!inode || !inode->i_sb || !inode->i_sb->s_op || !inode->i_sb->s_op->statfs)
		return -ENOSYS;

	/* do statfs */
	inode->i_sb->s_op->statfs(inode->i_sb, buf);

	return 0;
}

/*
 * Statfs system call.
 */
int sys_statfs64(const char *path, size_t size, struct statfs64 *buf)
{
	struct dentry *dentry;
	int ret;

	/* check buffer size */
	if (size != sizeof(*buf))
		return -EINVAL;

	/* resolve path */
	dentry = namei(AT_FDCWD, NULL, path, 1);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* do statfs */
	ret = do_statfs64(dentry->d_inode, buf);

	dput(dentry);
	return ret;
}

/*
 * Fstatfs system call.
 */
int sys_fstatfs64(int fd, size_t size, struct statfs64 *buf)
{
	struct file *filp;
	int ret;

	/* check output buffer */
	if (!buf)
		return -EINVAL;

	/* check size */
	if (size != sizeof(*buf))
		return -EINVAL;

	/* get file */
	filp = fget(fd);
	if (!filp)
		return -EBADF;

	/* do statfs */
	ret = do_statfs64(filp->f_inode, buf);

	/* release file */
	fput(filp);

	return ret;
}
