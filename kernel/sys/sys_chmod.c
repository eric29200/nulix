#include <sys/syscall.h>

/*
 * Chmod system call.
 */
int sys_chmod(const char *pathname, mode_t mode)
{
  return do_chmod(pathname, mode);
}
