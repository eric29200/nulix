#include <sys/syscall.h>
#include <stderr.h>

/*
 * Llseek system call.
 */
int sys_llseek(int fd, uint32_t offset_high, uint32_t offset_low, off_t *result, int whence)
{
	off_t offset;

	/* check file descriptor */
	if (fd >= NR_OPEN || fd < 0 || !current_task->files->filp[fd])
		return -EBADF;

	/* compute offset */
	offset = ((unsigned long long) offset_high << 32) | offset_low;

	/* seek */
	*result = do_lseek(current_task->files->filp[fd], offset, whence);

	return 0;
}
