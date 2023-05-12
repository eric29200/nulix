#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "../x86/__syscall.h"

int fstatat(int dirfd, const char *pathname, struct stat *statbuf, int flags)
{
	return syscall4(SYS_fstatat64, dirfd, (long) pathname, (long) statbuf, flags);
}
