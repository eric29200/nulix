#include <sys/syscall.h>

/*
 * Chroot system call.
 */
int sys_chroot(const char *path)
{
	return do_chroot(path);
}
