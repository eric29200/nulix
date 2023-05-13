
#include <unistd.h>

#include "../x86/__syscall.h"

int fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, int flags)
{
	return syscall5(SYS_fchownat, dirfd, (long) pathname, owner, group, flags);
}