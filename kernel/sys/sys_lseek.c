#include <sys/syscall.h>

/*
 * Lseek system call.
 */
off_t sys_lseek(int fd, off_t offset, int whence)
{
	return do_lseek(fd, offset, whence);
}
