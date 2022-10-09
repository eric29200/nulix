#include <sys/syscall.h>

/*
 * Pread64 system call.
 */
int sys_pread64(int fd, void *buf, size_t count, off_t offset)
{
	return do_pread64(fd, buf, count, offset);
}
