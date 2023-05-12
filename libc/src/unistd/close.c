#include <unistd.h>

#include "../x86/__syscall.h"

int close(int fd)
{
	return syscall1(SYS_close, fd);
}