#include <unistd.h>

#include "../x86/__syscall.h"

int open(const char *pathname, int flags, mode_t mode)
{
	return __syscall3(SYS_open, (long) pathname, flags, mode);
}