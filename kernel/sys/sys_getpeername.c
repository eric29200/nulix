#include <sys/syscall.h>

/*
 * Get peer name system call.
 */
int sys_getpeername(int sockfd, struct sockaddr *addr, size_t *addrlen)
{
  return do_getpeername(sockfd, addr, addrlen);
}
