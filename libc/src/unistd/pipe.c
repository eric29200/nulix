#include <unistd.h>

#include "../x86/__syscall.h"

int pipe(int fd[2])
{
	return syscall1(SYS_pipe, (long) fd);
}