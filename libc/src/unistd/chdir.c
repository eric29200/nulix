#include <unistd.h>

#include "../x86/__syscall.h"

int chdir(const char *path)
{
	return syscall1(SYS_chdir, (long) path);
}