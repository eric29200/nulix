#include <sys/syscall.h>

/*
 * Select system call.
 */
int sys_select(int nfds, fd_set_t *readfds, fd_set_t *writefds, fd_set_t *exceptfds, struct timeval_t *timeout)
{
  return do_select(nfds, readfds, writefds, exceptfds, timeout);
}
