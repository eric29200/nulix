#include <sys/syscall.h>

/*
 * Set groups system call.
 */
int sys_setgroups(size_t size, const gid_t *list)
{
  UNUSED(list);
  return size;
}
