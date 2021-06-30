#include <sys/syscall.h>

/*
 * Set pointer to thread id system call.
 */
pid_t sys_set_tid_address(int *tidptr)
{
  UNUSED(tidptr);
  return current_task->pid;
}
