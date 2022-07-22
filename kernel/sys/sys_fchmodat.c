#include <sys/syscall.h>
#include <fcntl.h>

/*
 * Fchmodat system call.
 */
int sys_fchmodat(int dirfd, const char *pathname, mode_t mode, unsigned int flags)
{
	UNUSED(flags);
	return do_chmod(dirfd, pathname, mode);
}
