#include <sys/syscall.h>
#include <fcntl.h>

/*
 * Link system call (make a new name for a file = hard link).
 */
int sys_link(const char *oldpath, const char *newpath)
{
	return do_link(AT_FDCWD, oldpath, AT_FDCWD, newpath);
}
