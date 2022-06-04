#include <sys/syscall.h>
#include <fcntl.h>

/*
 * Unlink system call.
 */
int sys_unlink(const char *pathname)
{
	return do_unlink(AT_FDCWD, pathname);
}
