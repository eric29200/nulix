#include <unistd.h>

#include "../x86/__syscall.h"

uid_t getuid()
{
	return syscall0(SYS_getuid);
}
