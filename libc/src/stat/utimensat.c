#include <sys/stat.h>
#include <unistd.h>

#include "../x86/__syscall.h"

#define NS_SPECIAL(ns)		((ns) == UTIME_NOW || (ns) == UTIME_OMIT)

int utimensat(int dirfd, const char *pathname, const struct timespec times[2], int flags)
{
	time_t s0 = 0, s1 = 0;
	long ns0 = 0, ns1 = 0;

	if (times && times[0].tv_nsec == UTIME_NOW && times[1].tv_nsec == UTIME_NOW)
		times = 0;

	if (times) {
		ns0 = times[0].tv_nsec;
		ns1 = times[1].tv_nsec;

		if (!NS_SPECIAL(ns0))
			s0 = times[0].tv_sec;
		if (!NS_SPECIAL(ns1))
			s1 = times[1].tv_sec;
	}

	return syscall4(SYS_utimensat, dirfd, (long) pathname, times ? (long) (((long[]) { s0, ns0, s1, ns1 })) : 0, flags);
}