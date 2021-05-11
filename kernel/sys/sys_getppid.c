#include <sys/syscall.h>

/*
 * Get parent pid system call.
 */
pid_t sys_getppid()
{
  if (current_task->parent)
    return current_task->parent->pid;

  /* init process : no father */
  return current_task->pid;
}
