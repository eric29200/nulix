#include <sys/syscall.h>

/*
 * Pipe system call.
 */
int sys_pipe(int pipefd[2])
{
	return do_pipe(pipefd, 0);
}
