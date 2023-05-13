#include <time.h>
#include <unistd.h>

#include "../x86/__syscall.h"

int clock_gettime(clockid_t clockid, struct timespec *tp)
{
	return syscall2(SYS_clock_gettime64, clockid, (long) tp);
}