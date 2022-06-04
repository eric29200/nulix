#include <sys/syscall.h>
#include <fcntl.h>

/*
 * Open system call.
 */
int sys_open(const char *pathname, int flags, mode_t mode)
{
	return do_open(AT_FDCWD, pathname, flags, mode);
}
