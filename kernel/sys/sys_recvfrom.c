#include <sys/syscall.h>

/*
 * Receive from system call.
 */
int sys_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, size_t addrlen)
{
  return do_recvfrom(sockfd, buf, len, flags, src_addr, addrlen);
}
