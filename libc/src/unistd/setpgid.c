#include <unistd.h>

#include "../x86/__syscall.h"

int setpgid(pid_t pid, pid_t pgid)
{
	return syscall2(SYS_setpgid, pid, pgid);
}