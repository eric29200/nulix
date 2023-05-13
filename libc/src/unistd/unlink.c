#include <unistd.h>

#include "../x86/__syscall.h"

int unlink(const char *pathname)
{
	return syscall1(SYS_unlink, (long) pathname);
}