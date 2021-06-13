#include <sys/syscall.h>

/*
 * Poll system call.
 */
int sys_poll(struct pollfd_t *fds, size_t nfds, int timeout)
{
  return do_poll(fds, nfds, timeout);
}
