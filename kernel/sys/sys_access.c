#include <sys/syscall.h>

/*
 * Access system call.
 */
int sys_access(const char *filename)
{
  return do_access(filename);
}
