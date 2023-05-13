#include <unistd.h>

#include "../x86/__syscall.h"

gid_t getgid()
{
	return syscall0(SYS_getgid);
}
