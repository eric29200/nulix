#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

#include "../x86/__syscall.h"

int chmod(const char *pathname, mode_t mode)
{
	return syscall2(SYS_chmod, (long) pathname, mode);
}