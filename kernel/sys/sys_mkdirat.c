#include <sys/syscall.h>

/*
 * Mkdirat system call.
 */
int sys_mkdirat(int dirfd, const char *pathname, mode_t mode)
{
  return do_mkdir(dirfd, pathname, mode);
}
