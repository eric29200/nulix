#include <sys/syscall.h>

/*
 * Get directory entries system call.
 */

int sys_getdents64(int fd, void *dirp, size_t count)
{
	return do_getdents64(fd, dirp, count);
}
