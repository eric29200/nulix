#include <sys/syscall.h>

/*
 * Chown system call.
 */
int sys_chown(const char *pathname, uid_t owner, gid_t group)
{
	return do_chown(pathname, owner, group);
}
