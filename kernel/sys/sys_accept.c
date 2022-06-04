#include <sys/syscall.h>

/*
 * Accept system call.
 */
int sys_accept(int sockfd, struct sockaddr *addr, size_t addrlen)
{
	return do_accept(sockfd, addr, addrlen);
}
