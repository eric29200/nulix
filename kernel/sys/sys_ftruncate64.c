#include <sys/syscall.h>

/*
 * File truncate system call.
 */
int sys_ftruncate64(int fd, off_t length)
{
  return do_ftruncate(fd, length);
}
