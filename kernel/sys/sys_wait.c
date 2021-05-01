#include <sys/syscall.h>
#include <proc/sched.h>
#include <stderr.h>

/*
 * Wait system call.
 */
int sys_wait()
{
  struct list_head_t *pos;
  struct task_t *task;
  int has_children;
  int child_pid;

  for (;;) {
    has_children = 0;

    /* search zombie child */
    list_for_each(pos, &current_task->list) {
      task = list_entry(pos, struct task_t, list);
      if (task->parent != current_task)
        continue;

      has_children = 1;

      /* destroy first zombie task */
      if (task->state == TASK_ZOMBIE) {
        child_pid = task->pid;
        destroy_task(task);
        return child_pid;
      }
    }

    /* no children : return error */
    if (!has_children)
      return -ESRCH;

    /* else wait for child */
    task_sleep(current_task);
  }
}
