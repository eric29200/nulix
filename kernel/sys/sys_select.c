#include <sys/syscall.h>

/*
 * Select system call.
 */
int sys_select(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds, struct timeval_t *timeout)
{
	struct timespec_t ts;

	if (timeout) {
		ts.tv_sec = timeout->tv_sec;
		ts.tv_nsec = timeout->tv_usec * 1000L;
	}

	return do_select(nfds, readfds, writefds, exceptfds, timeout ? &ts : NULL);
}
