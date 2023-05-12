#include <unistd.h>

#include "../x86/__syscall.h"

int dup(int oldfd)
{
	return syscall1(SYS_dup, oldfd);
}