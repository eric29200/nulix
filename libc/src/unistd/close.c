#include <unistd.h>

#include "../x86/__syscall.h"

int close(int fd)
{
	return __syscall1(SYS_close, fd);
}