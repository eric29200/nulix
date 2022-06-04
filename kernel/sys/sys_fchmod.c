#include <sys/syscall.h>

/*
 * Fchmod system call.
 */
int sys_fchmod(int fd, mode_t mode)
{
	return do_fchmod(fd, mode);
}
