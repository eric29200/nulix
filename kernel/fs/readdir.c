#include <fs/fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <string.h>

/*
 * Get directory entries system call.
 */
int do_getdents64(int fd, void *dirp, size_t count)
{
	struct file_t *filp;

	/* check fd */
	if (fd < 0 || fd >= NR_OPEN || !current_task->filp[fd])
		return -EINVAL;
	filp = current_task->filp[fd];

	/* getdents not implemented */
	if (!filp->f_op || !filp->f_op->getdents64)
		return -EPERM;

	return filp->f_op->getdents64(filp, dirp, count);
}
