#include <sys/syscall.h>

/*
 * Set group id system call.
 */
int sys_setgid(gid_t gid)
{
  current_task->gid = gid;
  return 0;
}
