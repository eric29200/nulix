#include <sys/syscall.h>
#include <fs/fs.h>

/*
 * Close system call.
 */
int sys_close(int fd)
{
  return do_close(fd);
}
