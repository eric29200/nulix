#include <unistd.h>

#include "../x86/__syscall.h"

int access(const char *pathname, int mode)
{
	return syscall2(SYS_access, (long) pathname, mode);
}