#include <sys/syscall.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Statfs system call.
 */
int sys_statfs64(const char *path, size_t size, struct statfs64_t *buf)
{
	struct inode_t *inode;
	int ret;

	/* check buffer size */
	if (size != sizeof(*buf))
		return -EINVAL;

	/* get inode */
	inode = namei(AT_FDCWD, NULL, path, 1);
	if (!inode)
		return -ENOENT;

	/* do statfs */
	ret = do_statfs64(inode, buf);

	/* release inode */
	iput(inode);

	return ret;
}
