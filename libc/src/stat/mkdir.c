#include <sys/stat.h>
#include <unistd.h>

#include "../x86/__syscall.h"

int mkdir(const char *pathname, mode_t mode)
{
	return syscall2(SYS_mkdir, (long) pathname, mode);
}