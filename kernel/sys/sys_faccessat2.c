#include <sys/syscall.h>

/*
 * Faccessat2 system call.
 */
int sys_faccessat2(int dirfd, const char *pathname, int flags)
{
	return do_faccessat(dirfd, pathname, flags);
}
