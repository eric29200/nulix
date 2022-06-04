#include <sys/syscall.h>
#include <stderr.h>

/*
 * Sigaction system call (= change action taken by a process on receipt of a specific signal).
 */
int sys_sigaction(int signum, const struct sigaction_t *act, struct sigaction_t *oldact)
{
	if (signum <= 0 || signum > NSIGS)
		return -EINVAL;

	/* SIGSTOP and SIGKILL actions can't be redefined */
	if (signum == SIGSTOP || signum == SIGKILL)
		return -EINVAL;

	/* save old action */
	if (oldact)
		*oldact = current_task->signals[signum - 1];

	/* set new action */
	current_task->signals[signum - 1] = *act;

	return 0;
}
