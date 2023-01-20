#include <sys/syscall.h>

/*
 * Umount2 system call.
 */
int sys_umount2(const char *target, int flags)
{
	return do_umount(target, flags);
}