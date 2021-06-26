#include <sys/syscall.h>

/*
 * Connect system call.
 */
int sys_connect(int sockfd, const struct sockaddr *addr, size_t addrlen)
{
  return do_connect(sockfd, addr, addrlen);
}
