#include <sys/syscall.h>

/*
 * Pipe2 system call.
 */
int sys_pipe2(int pipefd[2], int flags)
{
	return do_pipe(pipefd, flags);
}
