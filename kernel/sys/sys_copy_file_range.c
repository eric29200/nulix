#include <sys/syscall.h>

/*
 * Copy file range system call.
 */
int sys_copy_file_range(int fd_in, off64_t *off_in, int fd_out, off64_t *off_out, size_t len, unsigned int flags)
{
	UNUSED(fd_in);
	UNUSED(off_in);
	UNUSED(fd_out);
	UNUSED(off_out);
	UNUSED(len);
	UNUSED(flags);
	return 0;
}
