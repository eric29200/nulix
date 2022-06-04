#include <sys/syscall.h>
#include <stderr.h>

/*
 * Change blocked signals system call.
 */
int sys_rt_sigprocmask(int how, const sigset_t *set, sigset_t *oldset, size_t sigsetsize)
{
	UNUSED(sigsetsize);
	return sys_sigprocmask(how, set, oldset);
}
