#include <sys/syscall.h>
#include <stderr.h>

/*
 * Read system call.
 */
int sys_read(int fd, char *buf, int count)
{
	/* check file descriptor */
	if (fd >= NR_OPEN || fd < 0 || count < 0 || !current_task->files->filp[fd])
		return -EBADF;

	return do_read(current_task->files->filp[fd], buf, count);
}
