#include <sys/syscall.h>
#include <fcntl.h>

/*
 * Chown system call.
 */
int sys_chown(const char *pathname, uid_t owner, gid_t group)
{
	return do_chown(AT_FDCWD, pathname, owner, group, 0);
}
