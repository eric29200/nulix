#include <sys/syscall.h>

/*
 * Receive a message system call.
 */
int sys_recvmsg(int sockfd, struct msghdr_t *msg, int flags)
{
  return do_recvmsg(sockfd, msg, flags);
}
