#include <fs/fs.h>
#include <proc/sched.h>
#include <fcntl.h>
#include <stderr.h>
#include <stdio.h>

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
	struct dentry *dentry, *tmp;
	struct inode *inode;
	struct file *filp;
	int ret;
	
	/* gt file */
	filp = fget(fd);
	if (!filp)
		return -EINVAL;

	/* get dentry */
	ret = -ENOENT;
	dentry = filp->f_dentry;
	if (!dentry)
		goto out;

	/* get inode */
	inode = dentry->d_inode;
	if (!inode)
		goto out;

	/* fd must be a directory */
	ret = -ENOTDIR;
	if (!S_ISDIR(inode->i_mode))
		goto out;

	/* check permissions */
	ret = permission(inode, MAY_EXEC);
	if (ret)
		goto out;

	/* change directory */
	tmp = current_task->fs->pwd;
	current_task->fs->pwd = dget(dentry);
	dput(tmp);
out:
	fput(filp);
	return ret;
}
