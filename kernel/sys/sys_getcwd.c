#include <sys/syscall.h>

/*
 * Get current working dir system call.
 */
int sys_getcwd(char *buf, size_t size)
{
  return -1;
  return do_getcwd(buf, size);
}
