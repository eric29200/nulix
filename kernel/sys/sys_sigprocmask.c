#include <sys/syscall.h>
#include <stderr.h>

/*
 * Change blocked signals system call.
 */
int sys_sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
	return do_sigprocmask(how, set, oldset);
}
