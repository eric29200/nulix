#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <sys/select.h>

#include "../x86/__syscall.h"

int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)
{
	const time_t max_time = (1ULL << (8 * sizeof(time_t) - 1)) - 1;
	time_t us = timeout ? timeout->tv_usec : 0;
	time_t s = timeout ? timeout->tv_sec : 0;

	/* check timeout */
	if (s < 0 || us < 0)
		return __syscall_ret(-EINVAL);

	if (us / 1000000 > max_time - s) {
		s = max_time;
		us = 999999;
	} else {
		s += us/1000000;
		us %= 1000000;
	}

	return syscall5(SYS_select, nfds, (long) readfds, (long) writefds, (long) exceptfds, timeout ? (long) ((long[]) { s, us }) : 0);
}