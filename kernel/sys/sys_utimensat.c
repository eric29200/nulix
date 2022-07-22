#include <sys/syscall.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Utimensat system call.
 */
int sys_utimensat(int dirfd, const char *pathname, struct timespec_t *times, int flags)
{
	return do_utimensat(dirfd, pathname, times, flags);
}
