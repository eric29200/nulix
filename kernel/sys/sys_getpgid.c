#include <sys/syscall.h>

/*
 * Get process group id system call.
 */
pid_t sys_getpgid(pid_t pid)
{
  struct task_t *task;

  /* get matching task or current task */
  if (pid == 0)
    task = current_task;
  else
    task = get_task(pid);

  /* no matching task */
  if (task == NULL)
    return -1;

  /* get process group id */
  return task->pgid;
}
