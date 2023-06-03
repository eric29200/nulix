#include <sys/syscall.h>
#include <stderr.h>

/*
 * Pread64 system call.
 */
int sys_pread64(int fd, void *buf, size_t count, off_t offset)
{
	/* check file descriptor */
	if (fd >= NR_OPEN || fd < 0 || !current_task->files->filp[fd])
		return -EBADF;

	return do_pread64(current_task->files->filp[fd], buf, count, offset);
}
