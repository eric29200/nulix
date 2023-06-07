#include <sys/syscall.h>

int sys_rt_sigsuspend(sigset_t *newset, size_t sigsetsize)
{
	UNUSED(sigsetsize);
	return do_sigsuspend(newset);
}
