#include <unistd.h>
#include <time.h>

#include "../x86/__syscall.h"

int nanosleep(const struct timespec *req, struct timespec *rem)
{
	long ts32[2] = { req->tv_sec, req->tv_nsec };
	int ret;

	/* nanosleep system call, with old timespec (long, long )*/
	ret = __syscall2(SYS_nanosleep, (long) &ts32, (long) &ts32);

	/* convert back remaining */
	if (ret == -EINTR && rem) {
		rem->tv_sec = ts32[0];
		rem->tv_nsec = ts32[1];
	}

	return __syscall_ret(ret);
}