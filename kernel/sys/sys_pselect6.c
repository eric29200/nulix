#include <sys/syscall.h>

/*
 * Pselect6 system call.
 */
int sys_pselect6(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds, struct timespec_t *timeout, sigset_t *sigmask)
{
	UNUSED(sigmask);
	return do_select(nfds, readfds, writefds, exceptfds, timeout);
}
