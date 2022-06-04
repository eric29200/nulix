#include <sys/syscall.h>
#include <fcntl.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Fcntl system call.
 */
int sys_fcntl(int fd, int cmd, unsigned long arg)
{
	return do_fcntl(fd, cmd, arg);
}
