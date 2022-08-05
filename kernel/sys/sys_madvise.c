#include <sys/syscall.h>

/*
 * Madvise system call.
 */
int sys_madvise(void *addr, size_t length, int advice)
{
	UNUSED(addr);
	UNUSED(length);
	UNUSED(advice);
	return 0;
}
