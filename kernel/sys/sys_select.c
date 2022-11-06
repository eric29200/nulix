#include <sys/syscall.h>

/*
 * Select system call.
 */
int sys_select(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds, struct old_timeval_t *timeout)
{
	struct kernel_timeval_t tv;

	if (timeout)
		old_timeval_to_kernel_timeval(timeout, &tv);

	return do_select(nfds, readfds, writefds, exceptfds, timeout ? &tv : NULL);
}
