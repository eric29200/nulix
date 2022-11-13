#include <sys/syscall.h>
#include <stderr.h>

/*
 * Get rusage system call.
 */
int sys_getrusage(int who, struct rusage_t *ru)
{
	if (who != RUSAGE_SELF && who != RUSAGE_CHILDREN)
		return -EINVAL;

	/* reset rusage */
	memset(ru, 0, sizeof(struct rusage_t));

	return 0;
}
