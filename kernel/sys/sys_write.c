#include <sys/syscall.h>
#include <fs/fs.h>

/*
 * Write system call.
 */
int sys_write(int fd, const char *buf, int count)
{
  return do_write(fd, buf, count);
}
