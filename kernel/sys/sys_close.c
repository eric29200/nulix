#include <sys/syscall.h>
#include <fs/fs.h>
#include <stderr.h>

/*
 * Close system call.
 */
int sys_close(int fd)
{
	int ret;

	/* check file descriptor */
	if (fd < 0 || fd >= NR_OPEN || !current_task->files->filp[fd])
		return -EINVAL;

	/* close file */
	ret = do_close(current_task->files->filp[fd]);
	if (ret)
		return ret;

	FD_CLR(fd, &current_task->files->close_on_exec);
	current_task->files->filp[fd] = NULL;
	return 0;
}
