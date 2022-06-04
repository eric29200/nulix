#include <sys/syscall.h>
#include <net/socket.h>

/*
 * Send to system call.
 */
int sys_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, size_t addrlen)
{
	return do_sendto(sockfd, buf, len, flags, dest_addr, addrlen);
}
