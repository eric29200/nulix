#include <sys/syscall.h>
#include <fs/fs.h>
#include <stderr.h>

/*
 * Write system call.
 */
int sys_write(int fd, const char *buf, int count)
{
	/* check file descriptor */
	if (fd >= NR_OPEN || fd < 0 || count < 0 || !current_task->files->filp[fd])
		return -EBADF;

	return do_write(current_task->files->filp[fd], buf, count);
}
