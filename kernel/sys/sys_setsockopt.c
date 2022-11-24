#include <sys/syscall.h>

/*
 * Set socket options system call.
 */
int sys_setsockopt(int sockfd, int level, int optname, void *optval, size_t optlen)
{
	return do_setsockopt(sockfd, level, optname, optval, optlen);
}
