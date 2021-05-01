#include <sys/syscall.h>
#include <proc/sched.h>

/*
 * Exit a task.
 */
void sys_exit(int status)
{
  struct list_head_t *pos;
  struct task_t *child;
  int i;

  /* close opened files */
  for (i = 0; i < NR_OPEN; i++)
    if (current_task->filp[i])
      sys_close(i);

  /* mark task terminated and reschedule */
  current_task->state = TASK_ZOMBIE;
  current_task->exit_code = status;

  /* wakeup parent */
  task_wakeup(current_task->parent);

  /* give children to init */
  list_for_each(pos, &current_task->list) {
    child = list_entry(pos, struct task_t, list);
    if (child->parent == current_task) {
      child->parent = init_task;
      if (child->state == TASK_ZOMBIE)
        task_wakeup(init_task);
    }
  }

  /* call scheduler */
  schedule();
}
