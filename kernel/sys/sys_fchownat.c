#include <sys/syscall.h>
#include <fcntl.h>

/*
 * Fchownat system call.
 */
int sys_fchownat(int dirfd, const char *pathname, uid_t owner, gid_t group, unsigned int flags)
{
	return do_chown(dirfd, pathname, owner, group, flags);
}
