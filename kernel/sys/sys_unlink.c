#include <sys/syscall.h>

/*
 * Unlink system call.
 */
int sys_unlink(const char *pathname)
{
  return do_unlink(pathname);
}
