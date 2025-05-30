#include <fs/fs.h>
#include <proc/sched.h>
#include <fcntl.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Faccessat system call.
 */
static int do_faccessat(int dirfd, const char *pathname, mode_t mode, int flags)
{
	struct inode *inode;
	int ret;

	/* check inode */
	ret = namei(dirfd, NULL, pathname, flags & AT_SYMLINK_NO_FOLLOW ? 0 : 1, &inode);
	if (ret)
		return ret;

	/* check permissions */
	ret = permission(inode, mode);
	iput(inode);

	return ret;
}

/*
 * Access system call.
 */
int sys_access(const char *filename, mode_t mode)
{
	return do_faccessat(AT_FDCWD, filename, mode, 0);
}

/*
 * Faccessat system call.
 */
int sys_faccessat(int dirfd, const char *pathname, int flags)
{
	return do_faccessat(dirfd, pathname, 0, flags);
}

/*
 * Faccessat2 system call.
 */
int sys_faccessat2(int dirfd, const char *pathname, int flags)
{
	return do_faccessat(dirfd, pathname, 0, flags);
}

/*
 * Fadvise system call (ignore this system call).
 */
int sys_fadvise64(int fd, off_t offset, off_t len, int advice)
{
	UNUSED(fd);
	UNUSED(offset);
	UNUSED(len);
	UNUSED(advice);
	return 0;
}
