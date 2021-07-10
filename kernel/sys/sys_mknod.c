#include <sys/syscall.h>
#include <fcntl.h>

/*
 * Mknod system call.
 */
int sys_mknod(const char *pathname, mode_t mode, dev_t dev)
{
  return do_mknod(AT_FDCWD, pathname, mode, dev);
}
