#include <sys/syscall.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Fstat system call.
 */
int sys_fstat(int fd, struct stat_t *statbuf)
{
	/* check fd */
	if (fd >= NR_OPEN || !current_task->filp[fd])
		return -EINVAL;

	return do_stat(current_task->filp[fd]->f_inode, statbuf);
}
