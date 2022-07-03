#include <sys/syscall.h>
#include <stderr.h>

/*
 * Statfs system call.
 */
int sys_statfs64(const char *path, size_t size, struct statfs64_t *buf)
{
	/* check buffer size */
	if (size != sizeof(*buf))
		return -EINVAL;

	return do_statfs64(path, buf);
}
