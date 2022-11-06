#include <sys/syscall.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Utimensat system call.
 */
int sys_utimensat(int dirfd, const char *pathname, struct timespec_t *times, int flags)
{
	struct kernel_timeval_t ktimes[2];

	/* convert times to kernel timevals */
	if (times) {
		timespec_to_kernel_timeval(&times[0], &ktimes[0]);
		timespec_to_kernel_timeval(&times[1], &ktimes[1]);
	}

	return do_utimensat(dirfd, pathname, times ? ktimes : NULL, flags);
}
