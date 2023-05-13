#include <unistd.h>

#include "../x86/__syscall.h"

uid_t geteuid()
{
	return syscall0(SYS_geteuid);
}
