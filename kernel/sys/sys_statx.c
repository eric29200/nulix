#include <sys/syscall.h>

/*
 * Statx system call.
 */
int sys_statx(int dirfd, const char *pathname, int flags, unsigned int mask, struct statx_t *statbuf)
{
	return do_statx(dirfd, pathname, flags, mask, statbuf);
}
