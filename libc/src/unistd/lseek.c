#include <unistd.h>

#include "../x86/__syscall.h"

off_t lseek(int fd, off_t offset, int whence)
{
	return syscall3(SYS_lseek, fd, offset, whence);
}