#include <unistd.h>

#include "../x86/__syscall.h"

ssize_t read(int fd, void *buf, size_t count)
{
	return syscall3(SYS_read, fd, (long) buf, count);
}