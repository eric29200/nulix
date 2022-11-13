#include <sys/syscall.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Get time system call.
 */
int sys_clock_gettime64(clockid_t clockid, struct timespec_t *tp)
{
	switch (clockid) {
		case CLOCK_REALTIME:
			tp->tv_sec = startup_time + xtimes.tv_sec;
			((uint32_t *) &tp->tv_nsec)[0] = xtimes.tv_nsec;
			((uint32_t *) &tp->tv_nsec)[1] = 0;
			break;
		case CLOCK_MONOTONIC:
			tp->tv_sec = xtimes.tv_sec;
			((uint32_t *) &tp->tv_nsec)[0] = xtimes.tv_nsec;
			((uint32_t *) &tp->tv_nsec)[1] = 0;
			break;
		default:
			printf("clock_gettime64 not implement on clockid=%d\n", clockid);
			return -ENOSYS;
	}

	return 0;
}
