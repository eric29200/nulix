#include <sys/syscall.h>

/*
 * Rmdir system call.
 */
int sys_rmdir(const char *pathname)
{
  return do_rmdir(pathname);
}
