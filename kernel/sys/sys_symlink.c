#include <sys/syscall.h>
#include <fcntl.h>

/*
 * Create symbolic link system call.
 */
int sys_symlink(const char *target, const char *linkpath)
{
  return do_symlink(target, AT_FDCWD, linkpath);
}
