#include <sys/syscall.h>

/*
 * Get directory entries system call.
 */
int sys_getdents(int fd, struct dirent_t *dirent, uint32_t count)
{
  return do_getdents(fd, dirent, count);
}
