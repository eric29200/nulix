#include <sys/wait.h>
#include <unistd.h>

#include "../x86/__syscall.h"

pid_t waitpid(pid_t pid, int *wstatus, int options)
{
	return syscall4(SYS_wait4, pid, (long) wstatus, options, 0);
}