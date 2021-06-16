#include <sys/syscall.h>

/*
 * Bind system call.
 */
int sys_bind(int sockfd, const struct sockaddr *addr, size_t addrlen)
{
  return do_bind(sockfd, addr, addrlen);
}
