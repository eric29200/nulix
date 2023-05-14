#include <stdio.h>
#include <unistd.h>

#include "../x86/__syscall.h"

int rename(const char *oldpath, const char *newpath)
{
	return syscall2(SYS_rename, (long) oldpath, (long) newpath);
}