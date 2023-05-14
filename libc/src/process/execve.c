#include <unistd.h>

#include "../x86/__syscall.h"

int execve(const char *file, char *const argv[], char *const envp[])
{
	return syscall3(SYS_execve, (long) file, (long) argv, (long) envp);
}