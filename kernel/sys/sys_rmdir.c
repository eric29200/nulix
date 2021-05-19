#include <sys/syscall.h>
#include <fcntl.h>

/*
 * Rmdir system call.
 */
int sys_rmdir(const char *pathname)
{
  return do_rmdir(AT_FDCWD, pathname);
}
