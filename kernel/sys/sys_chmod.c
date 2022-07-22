#include <sys/syscall.h>
#include <fcntl.h>

/*
 * Chmod system call.
 */
int sys_chmod(const char *pathname, mode_t mode)
{
	return do_chmod(AT_FDCWD, pathname, mode);
}
