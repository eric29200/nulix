#include <sys/syscall.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Fchdir system call.
 */
int sys_fchdir(int fd)
{
	struct inode_t *inode;

	/* check fd */
	if (fd >= NR_OPEN || !current_task->files->filp[fd])
		return -EINVAL;

	/* fd must be a directory */
	inode = current_task->files->filp[fd]->f_inode;
	if (!S_ISDIR(inode->i_mode))
		return -ENOTDIR;

	/* release current working dir */
	iput(current_task->fs->cwd);

	/* set current working dir */
	current_task->fs->cwd = inode;

	return 0;
}
