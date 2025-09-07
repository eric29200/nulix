#include <fs/fs.h>
#include <proc/sched.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Change directory system call.
 */
int sys_chdir(const char *path)
{
	struct dentry *dentry;
	struct inode *inode;
	int ret;

	/* resolve path */
	dentry = namei(AT_FDCWD, NULL, path, 1);
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

	/* release current working dir */
	iput(current_task->fs->cwd);

	/* set current working dir */
	current_task->fs->cwd = inode;
	inode->i_count++;
out:
	dput(dentry);
	return ret;
}

/*
 * Fchdir system call.
 */
int sys_fchdir(int fd)
{
	struct inode *inode;
	struct file *filp;
	int ret;

	/* gt file */
	filp = fget(fd);
	if (!filp)
		return -EINVAL;

	/* fd must be a directory */
	inode = filp->f_inode;
	if (!S_ISDIR(inode->i_mode)) {
		fput(filp);
		return -ENOTDIR;
	}

	/* check permissions */
	ret = permission(inode, MAY_EXEC);
	if (ret) {
		fput(filp);
		return ret;
	}

	/* no change */
	if (inode == current_task->fs->cwd) {
		fput(filp);
		return 0;
	}

	/* release current working dir */
	iput(current_task->fs->cwd);

	/* set current working dir */
	current_task->fs->cwd = inode;

	/* release file */
	fput(filp);

	return 0;
}
