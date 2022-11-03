#include <sys/syscall.h>

/*
 * Pselect6 system call.
 */
int sys_pselect6(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds, struct timespec_t *timeout, sigset_t *sigmask)
{
	sigset_t current_sigmask;
	int ret;

	/* save current sigmask and set new sigmask */
	if (sigmask) {
		current_sigmask = current_task->sigmask;
		current_task->sigmask = *sigmask;
	}

	/* select */
	ret = do_select(nfds, readfds, writefds, exceptfds, timeout);

	/* restore sigmask and delete masked pending signals */
	if (sigmask) {
		current_task->sigmask = current_sigmask;
		current_task->sigpend &= !*sigmask;
	}

	return ret;
}
