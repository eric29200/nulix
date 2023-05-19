#include <unistd.h>

#include "../x86/__syscall.h"

pid_t getpid()
{
	return syscall0(SYS_getpid);
}