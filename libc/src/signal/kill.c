#include <signal.h>
#include <unistd.h>

#include "../x86/__syscall.h"

int kill(pid_t pid, int sig)
{
	return syscall2(SYS_kill, pid, sig);
}