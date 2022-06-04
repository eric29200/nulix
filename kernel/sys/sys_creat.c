#include <sys/syscall.h>
#include <fcntl.h>

/*
 * Creat system call.
 */
int sys_creat(const char *pathname, mode_t mode)
{
	return do_open(AT_FDCWD, pathname, O_CREAT | O_TRUNC, mode);
}
