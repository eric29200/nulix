#include <unistd.h>
#include <fcntl.h>

#include "../x86/__syscall.h"

int chown(const char *pathname, uid_t owner, gid_t group)
{
	return syscall5(SYS_fchownat, AT_FDCWD, (long) pathname, owner, group, 0);
}