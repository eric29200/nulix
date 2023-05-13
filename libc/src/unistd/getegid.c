#include <unistd.h>

#include "../x86/__syscall.h"

gid_t getegid()
{
	return syscall0(SYS_getegid);
}
