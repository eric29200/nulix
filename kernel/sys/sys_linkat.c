#include <sys/syscall.h>

/*
 * Linkat system call.
 */
int sys_linkat(int olddirfd, const char *oldpath, int newdirfd, const char *newpath, int flags)
{
  UNUSED(flags);
  return do_link(olddirfd, oldpath, newdirfd, newpath);
}
