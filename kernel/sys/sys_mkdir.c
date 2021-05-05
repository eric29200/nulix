#include <sys/syscall.h>

/*
 * Mkdir system call.
 */
int sys_mkdir(const char *filename, mode_t mode)
{
  return do_mkdir(filename, mode);
}
