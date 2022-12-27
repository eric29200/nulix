#include <sys/syscall.h>

/*
 * Fadvise system call (ignore this system call).
 */
int sys_fadvise64(int fd, off_t offset, off_t len, int advice)
{
	UNUSED(fd);
	UNUSED(offset);
	UNUSED(len);
	UNUSED(advice);
	return 0;
}
