#include <sys/syscall.h>
#include <stderr.h>

/*
 * Lseek system call.
 */
off_t sys_lseek(int fd, off_t offset, int whence)
{
	/* check file descriptor */
	if (fd >= NR_OPEN || fd < 0 || !current_task->files->filp[fd])
		return -EBADF;

	return do_lseek(current_task->files->filp[fd], offset, whence);
}
