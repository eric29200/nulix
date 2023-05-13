#include <sys/stat.h>
#include <unistd.h>

#include "../x86/__syscall.h"

int mknod(const char *pathname, mode_t mode, dev_t dev)
{
	return syscall3(SYS_mknod, (long) pathname, mode, dev);
}