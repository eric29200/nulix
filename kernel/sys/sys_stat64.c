#include <sys/syscall.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Stat64 system call.
 */
int sys_stat64(const char *pathname, struct stat64_t *statbuf)
{
	struct inode_t *inode;
	int ret;

	/* get inode */
	inode = namei(AT_FDCWD, NULL, pathname, 1);
	if (!inode)
		return -ENOENT;

	/* do stat */
	ret = do_stat64(inode, statbuf);

	/* release inode */
	iput(inode);
	return ret;
}
