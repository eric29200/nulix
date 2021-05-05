#include <sys/syscall.h>

/*
 * Open system call.
 */
int sys_open(const char *pathname, int flags, mode_t mode)
{
  return do_open(pathname, flags, mode);
}
