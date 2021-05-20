#include <sys/syscall.h>
#include <fcntl.h>

/*
 * Read a symbolic link system call.
 */
ssize_t sys_readlink(const char *pathname, char *buf, size_t bufsize)
{
  return do_readlink(AT_FDCWD, pathname, buf, bufsize);
}
