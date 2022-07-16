#include <fs/fs.h>
#include <proc/sched.h>
#include <fcntl.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Faccessat system call.
 */
int do_faccessat(int dirfd, const char *pathname, int flags)
{
	struct inode_t *inode;

	/* check inode */
	inode = namei(dirfd, NULL, pathname, flags & AT_SYMLINK_NO_FOLLOW ? 0 : 1);
	if (!inode)
		return -ENOENT;

	/* release inode */
	iput(inode);
	return 0;
}
