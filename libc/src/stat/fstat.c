
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "../x86/__syscall.h"

int fstat(int fd, struct stat *statbuf)
{
	if (fd < 0)
		return __syscall_ret(-EBADF);

	return fstatat(fd, "", statbuf, AT_EMPTY_PATH);
}