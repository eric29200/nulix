#include <sys/syscall.h>
#include <proc/sched.h>

/*
 * Exit a task.
 */
void sys_exit(int status)
{
  /* mark task terminated and reschedule */
  current_task->state = TASK_ZOMBIE;
  current_task->exit_code = status;

  /* call scheduler */
  schedule();
}
