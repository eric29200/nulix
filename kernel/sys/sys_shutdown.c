#include <sys/syscall.h>

/*
 * Shutdown system call.
 */
int sys_shutdown(int sockfd, int how)
{
	return do_shutdown(sockfd, how);
}
