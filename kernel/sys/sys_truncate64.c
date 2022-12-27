#include <sys/syscall.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Truncate system call.
 */
int sys_truncate64(const char *pathname, off64_t length)
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
