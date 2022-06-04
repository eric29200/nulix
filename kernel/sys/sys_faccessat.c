#include <sys/syscall.h>

/*
 * Faccessat system call.
 */
int sys_faccessat(int dirfd, const char *pathname, int flags)
{
	return do_faccessat(dirfd, pathname, flags);
}
