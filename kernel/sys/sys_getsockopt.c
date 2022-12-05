#include <sys/syscall.h>

/*
 * Get socket options system call.
 */
int sys_getsockopt(int sockfd, int level, int optname, void *optval, size_t optlen)
{
	return do_getsockopt(sockfd, level, optname, optval, optlen);
}
