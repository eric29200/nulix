#include <sys/syscall.h>

/*
 * Get sock name system call.
 */
int sys_getsockname(int sockfd, struct sockaddr *addr, size_t *addrlen)
{
	return do_getsockname(sockfd, addr, addrlen);
}
