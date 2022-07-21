#include <sys/syscall.h>
#include <stderr.h>

/*
 * Fstatfs system call.
 */
int sys_fstatfs64(int fd, struct statfs64_t *buf)
{
	struct file_t *filp;

	/* check output buffer */
	if (!buf)
		return -EINVAL;

	/* check input file */
	if (fd >= NR_OPEN || fd < 0 || !current_task->filp[fd])
		return -EBADF;

	/* get input file */
	filp = current_task->filp[fd];

	/* do statfs */
	return do_statfs64(filp->f_inode, buf);
}
