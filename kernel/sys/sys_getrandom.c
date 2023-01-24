#include <sys/syscall.h>
#include <drivers/char/random.h>

/*
 * Get random system call.
 */
int sys_getrandom(void *buf, size_t buflen, unsigned int flags)
{
	/* unused flags */
	UNUSED(flags);

	/* use /dev/random */
	return random_iops.fops->read(NULL, buf, buflen);
}
