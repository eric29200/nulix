#include <fs/fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <fcntl.h>

/*
 * Truncate system call.
 */
int do_truncate(struct inode_t *inode, off_t length)
{
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
	struct inode_t *inode;
	int ret;

	/* get inode */
	inode = namei(AT_FDCWD, NULL, pathname, 1);
	if (!inode)
		return -ENOENT;

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
	struct inode_t *inode;

	/* check file descriptor */
	if (fd >= NR_OPEN || fd < 0 || !current_task->files->filp[fd])
		return -EBADF;

	/* get inode */
	inode = current_task->files->filp[fd]->f_inode;

	return do_truncate(inode, length);
}

/*
 * File truncate system call.
 */
int sys_ftruncate64(int fd, off_t length)
{
	return do_ftruncate(fd, length);
}