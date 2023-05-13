#include <unistd.h>

#include "../x86/__syscall.h"

int rmdir(const char *pathname)
{
	return syscall1(SYS_rmdir, (long) pathname);
}