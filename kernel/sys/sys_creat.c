#include <sys/syscall.h>
#include <fcntl.h>

/*
 * Creat system call.
 */
int sys_creat(const char *pathname, mode_t mode)
{
  return do_open(pathname, O_CREAT | O_TRUNC, mode);
}
