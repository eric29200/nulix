#include <sys/syscall.h>

/*
 * Read a symbolic link system call.
 */
ssize_t sys_readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsize)
{
	return do_readlink(dirfd, pathname, buf, bufsize);
}
