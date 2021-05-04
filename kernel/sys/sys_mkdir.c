#include <sys/syscall.h>

/*
 * Mkdir system call.
 */
int sys_mkdir(const char *filename)
{
  return do_mkdir(filename);
}
