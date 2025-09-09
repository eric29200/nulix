#include <fs/fs.h>
#include <proc/sched.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Get current working dir system call.
 */
int sys_getcwd(char *buf, size_t size)
{
	UNUSED(buf);
	UNUSED(size);
	return -EINVAL;
}