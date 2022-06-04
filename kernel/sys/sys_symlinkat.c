#include <sys/syscall.h>

/*
 * Create symbolic link system call.
 */
int sys_symlinkat(const char *target, int newdirfd, const char *linkpath)
{
	return do_symlink(target, newdirfd, linkpath);
}
