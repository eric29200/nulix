#include <unistd.h>

#include "../x86/__syscall.h"

ssize_t write(int fd, const void *buf, size_t count)
{
	return __syscall3(SYS_write, fd, (long) buf, count);
}