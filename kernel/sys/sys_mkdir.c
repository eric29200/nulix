#include <sys/syscall.h>
#include <fcntl.h>

/*
 * Mkdir system call.
 */
int sys_mkdir(const char *filename, mode_t mode)
{
  return do_mkdir(AT_FDCWD, filename, mode);
}
