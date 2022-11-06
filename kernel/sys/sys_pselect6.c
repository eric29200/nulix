#include <sys/syscall.h>

/*
 * Pselect6 system call.
 */
int sys_pselect6(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds, struct timespec_t *timeout, sigset_t *sigmask)
{
	struct kernel_timeval_t tv;
	sigset_t current_sigmask;
	int ret;

	/* save current sigmask and set new sigmask */
	if (sigmask) {
		current_sigmask = current_task->sigmask;
		current_task->sigmask = *sigmask;
	}

	/* convert timespec to kernel timeval */
	if (timeout)
		timespec_to_kernel_timeval(timeout, &tv);

	/* select */
	ret = do_select(nfds, readfds, writefds, exceptfds, timeout ? &tv : NULL);

	/* restore sigmask and delete masked pending signals */
	if (sigmask) {
		current_task->sigmask = current_sigmask;
		current_task->sigpend &= !*sigmask;
	}

	return ret;
}
