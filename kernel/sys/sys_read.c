#include <sys/syscall.h>

/*
 * Read system call.
 */
int sys_read(int fd, char *buf, int count)
{
  return do_read(fd, buf, count);
}
