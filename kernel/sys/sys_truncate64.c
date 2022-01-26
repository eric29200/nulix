#include <sys/syscall.h>

/*
 * Truncate system call.
 */
int sys_truncate64(const char *pathname, off_t length)
{
  return do_truncate(pathname, length);
}
