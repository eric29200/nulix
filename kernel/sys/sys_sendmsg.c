#include <sys/syscall.h>

/*
 * Send a message system call.
 */
int sys_sendmsg(int sockfd, const struct msghdr_t *msg, int flags)
{
	return do_sendmsg(sockfd, msg, flags);
}
