#include <sys/syscall.h>

/*
 * Open system call.
 */
int sys_open(const char *pathname)
{
  return do_open(pathname);
}
