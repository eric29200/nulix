#include <sys/syscall.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Fstatat64 system call.
 */
int sys_fstatat64(int dirfd, const char *pathname, struct stat64_t *statbuf, int flags)
{
	struct inode_t *inode;
	int ret;

	/* get inode */
	inode = namei(dirfd, NULL, pathname, flags & AT_SYMLINK_NO_FOLLOW ? 0 : 1);
	if (!inode)
		return -ENOENT;

	/* do stat */
	ret = do_stat64(inode, statbuf);

	/* release inode */
	iput(inode);
	return ret;
}
