#include <sys/syscall.h>

/*
 * Fsync system call (don't do anything because all block buffers should be clean).
 */
int sys_fsync(int fd)
{
	UNUSED(fd);
	return 0;
}
