#include <sys/syscall.h>
#include <fcntl.h>

/*
 * Openat system call.
 */
int sys_openat(int dirfd, const char *pathname, int flags, mode_t mode)
{
  return do_open(dirfd, pathname, flags, mode);
}
