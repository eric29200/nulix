#include <fs/fs.h>
#include <proc/sched.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Change directory system call.
 */
int sys_chdir(const char *path)
{
	struct dentry *dentry, *tmp;
	struct inode *inode;
	int ret;

	/* resolve path */
	dentry = namei(AT_FDCWD, path, 1);
	if (IS_ERR(dentry))
		return PTR_ERR(dentry);

	/* get inode */
	inode = dentry->d_inode;

	/* check directory */
	ret = -ENOTDIR;
	if (!S_ISDIR(inode->i_mode))
		goto out;

	/* check permissions */
	ret = permission(inode, MAY_EXEC);
	if (ret)
		goto out;

	/* exchange dentries */
	tmp = current_task->fs->pwd;
	current_task->fs->pwd = dentry;
	dentry = tmp;
out:
	dput(dentry);
	return ret;
}

/*
 * Fchdir system call.
 */
int sys_fchdir(int fd)
{
	struct dentry *dentry;
	struct inode *inode;
	struct file *filp;
	int ret;

	/* gt file */
	filp = fget(fd);
	if (!filp)
		return -EINVAL;

	/* fd must be a directory */
	inode = filp->f_inode;
	ret = -ENOTDIR;
	if (!S_ISDIR(inode->i_mode))
		goto out;

	/* check permissions */
	ret = permission(inode, MAY_EXEC);
	if (ret)
		goto out;

	/* allocate a new dentry */
	dentry = d_alloc_root(inode);
	ret = -ENOMEM;
	if (!dentry)
		goto out;

	/* change directory */
	dput(current_task->fs->pwd);
	current_task->fs->pwd = dentry;
out:
	fput(filp);
	return ret;
}
