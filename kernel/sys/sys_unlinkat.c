#include <sys/syscall.h>
#include <fcntl.h>

/*
 * Unlink at system call.
 */
int sys_unlinkat(int dirfd, const char *pathname, int flags)
{
	if (flags & AT_REMOVEDIR)
		return do_rmdir(dirfd, pathname);

	return do_unlink(dirfd, pathname);
}
