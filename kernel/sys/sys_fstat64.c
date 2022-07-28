#include <sys/syscall.h>
#include <stderr.h>

/*
 * Fstat64 system call.
 */
int sys_fstat64(int fd, struct stat64_t *statbuf)
{
	/* check fd */
	if (fd >= NR_OPEN || !current_task->filp[fd])
		return -EINVAL;

	/* do stat */
	return do_stat64(current_task->filp[fd]->f_inode, statbuf);
}
