#include <sys/syscall.h>
#include <stderr.h>

/*
 * Pselect6 system call.
 */
int sys_pselect6(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds, struct timespec_t *timeout, sigset_t *sigmask)
{
	struct kernel_timeval_t tv;
	sigset_t current_sigmask;
	int ret;

	/* handle sigmask */
	if (sigmask) {
		/* save current sigmask */
		current_sigmask = current_task->sigmask;

		/* set new sigmask (do not mask SIGKILL and SIGSTOP) */
		current_task->sigmask = *sigmask;
		sigdelset(&current_task->sigmask, SIGKILL);
		sigdelset(&current_task->sigmask, SIGSTOP);
	}

	/* convert timespec to kernel timeval */
	if (timeout)
		timespec_to_kernel_timeval(timeout, &tv);

	/* select */
	ret = do_select(nfds, readfds, writefds, exceptfds, timeout ? &tv : NULL);

	/* restore sigmask and delete masked pending signals */
	if (ret == -EINTR && sigmask)
		current_task->saved_sigmask = current_sigmask;
	else if (sigmask)
		current_task->sigmask = current_sigmask;

	return ret;
}
