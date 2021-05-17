#include <sys/syscall.h>

/*
 * Link system call (make a new name for a file = hard link).
 */
int sys_link(const char *oldpath, const char *newpath)
{
  return do_link(oldpath, newpath);
}
