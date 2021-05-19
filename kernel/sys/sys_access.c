#include <sys/syscall.h>

/*
 * Access system call.
 */
int sys_access(const char *filename, mode_t mode)
{
  return do_access(filename, mode);
}
