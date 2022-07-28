#include <sys/syscall.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Lstat system call.
 */
int sys_lstat(const char *filename, struct stat_t *statbuf)
{
	struct inode_t *inode;
	int ret;

	/* get inode */
	inode = namei(AT_FDCWD, NULL, filename, 0);
	if (!inode)
		return -ENOENT;

	ret = do_stat(inode, statbuf);
	iput(inode);

	return ret;
}
