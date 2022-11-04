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
			jiffies_to_timespec(jiffies, tp);
			tp->tv_sec += startup_time;
			break;
		case CLOCK_MONOTONIC:
			jiffies_to_timespec(jiffies, tp);
			break;
		default:
			printf("clock_gettime64 not implement on clockid=%d\n", clockid);
			return -ENOSYS;
	}

	return 0;
}
