#include <sys/syscall.h>
#include <fcntl.h>

/*
 * Rename system call.
 */
int sys_rename(const char *oldpath, const char *newpath)
{
	return do_rename(AT_FDCWD, oldpath, AT_FDCWD, newpath, 0);
}
