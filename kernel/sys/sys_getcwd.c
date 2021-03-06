#include <sys/syscall.h>

/*
 * Get current working dir system call.
 */
int sys_getcwd(char *buf, size_t size)
{
	size_t n;

	/* check size limit */
	n = strlen(current_task->cwd_path);
	if (size <= n)
		n = size - 1;

	/* get current working dir path */
	strncpy(buf, current_task->cwd_path, n);
	buf[n] = 0;

	return n;
}
