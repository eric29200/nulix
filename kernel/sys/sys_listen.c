#include <sys/syscall.h>

/*
 * Listen system call.
 */
int sys_listen(int sockfd, int backlog)
{
  return do_listen(sockfd, backlog);
}
