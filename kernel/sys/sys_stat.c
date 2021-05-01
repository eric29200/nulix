#include <sys/syscall.h>

/*
 * Stat system call.
 */
int sys_stat(const char *filename, struct stat_t *statbuf)
{
  return do_stat(filename, statbuf);
}
