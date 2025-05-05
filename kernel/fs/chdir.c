#include <fs/fs.h>
#include <proc/sched.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Change directory system call.
 */
int sys_chdir(const char *path)
{
	struct inode *inode;
	int ret;

	/* get inode */
	ret = namei(AT_FDCWD, NULL, path, 1, &inode);
	if (ret)
		return ret;

	/* check directory */
	if (!S_ISDIR(inode->i_mode)) {
		iput(inode);
		return -ENOTDIR;
	}

	/* release current working dir */
	iput(current_task->fs->cwd);

	/* set current working dir */
	current_task->fs->cwd = inode;

	return 0;
}

/*
 * Fchdir system call.
 */
int sys_fchdir(int fd)
{
	struct inode *inode;

	/* check fd */
	if (fd >= NR_OPEN || !current_task->files->filp[fd])
		return -EINVAL;

	/* fd must be a directory */
	inode = current_task->files->filp[fd]->f_inode;
	if (!S_ISDIR(inode->i_mode))
		return -ENOTDIR;

	/* no change */
	if (inode == current_task->fs->cwd)
		return 0;

	/* release current working dir */
	iput(current_task->fs->cwd);

	/* set current working dir */
	current_task->fs->cwd = inode;

	return 0;
}
