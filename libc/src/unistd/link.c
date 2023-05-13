
#include <unistd.h>

#include "../x86/__syscall.h"

int link(const char *oldpath, const char *newpath)
{
	return syscall2(SYS_link, (long) oldpath, (long) newpath);
}