#include <sys/syscall.h>

/*
 * Fchown system call.
 */
int sys_fchown(int fd, uid_t owner, gid_t group)
{
  return do_fchown(fd, owner, group);
}
