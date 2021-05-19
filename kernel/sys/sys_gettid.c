#include <sys/syscall.h>

/*
 * Get thread id system call.
 */
pid_t sys_gettid()
{
  return current_task->pid;
}
