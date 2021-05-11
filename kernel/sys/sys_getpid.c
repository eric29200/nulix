#include <sys/syscall.h>

/*
 * Get pid system call.
 */
pid_t sys_getpid()
{
  return current_task->pid;
}
