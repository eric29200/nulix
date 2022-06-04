#include <sys/syscall.h>
#include <fcntl.h>

/*
 * Access system call.
 */
int sys_access(const char *filename, mode_t mode)
{
	UNUSED(mode);
	return do_faccessat(AT_FDCWD, filename, 0);
}
