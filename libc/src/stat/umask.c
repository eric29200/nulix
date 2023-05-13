#include <sys/stat.h>
#include <unistd.h>

#include "../x86/__syscall.h"

mode_t umask(mode_t mask)
{
	return syscall1(SYS_umask, mask);
}