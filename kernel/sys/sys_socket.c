#include <sys/syscall.h>
#include <net/socket.h>

/*
 * Socket system call.
 */
int sys_socket(int domain, int type, int protocol)
{
  return do_socket(domain, type, protocol);
}
