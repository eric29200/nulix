#include <unistd.h>

#include "../x86/__syscall.h"

int dup(int oldfd)
{
	return __syscall_ret(__syscall1(SYS_dup, oldfd));
}