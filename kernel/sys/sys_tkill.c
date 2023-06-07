#include <sys/syscall.h>
#include <stderr.h>

/*
 * Thread kill system call (send a signal to a process).
 */
int sys_tkill(pid_t pid, int sig)
{
	/* only valid for single tasks */
	if (pid <= 0)
		return -EINVAL;

	return sys_kill(pid, sig);
}
