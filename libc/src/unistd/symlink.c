#include <unistd.h>

#include "../x86/__syscall.h"

int symlink(const char *target, const char *linkpath)
{
	return syscall2(SYS_symlink, (long) target, (long) linkpath);
}