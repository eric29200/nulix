#include <unistd.h>

#include "../x86/__syscall.h"

pid_t fork()
{
	return __syscall0(SYS_fork);
}