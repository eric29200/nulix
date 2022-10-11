#include <sys/syscall.h>
#include <stderr.h>

/*
 * Clone system call.
 */
int sys_clone(uint32_t flags, uint32_t newsp)
{
	return do_fork(flags, newsp);
}
