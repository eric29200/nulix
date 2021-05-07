#include <sys/syscall.h>
#include <proc/sched.h>
#include <stderr.h>

/*
 * Wait system call.
 */
pid_t sys_waitpid(pid_t pid, int *wstatus, int options)
{
  struct list_head_t *pos;
  struct task_t *task;
  int has_children;
  int child_pid;

  /* unused options flags */
  UNUSED(options);

  /* look for a terminated child */
  for (;;) {
    has_children = 0;

    /* search zombie child */
    list_for_each(pos, &current_task->list) {
      task = list_entry(pos, struct task_t, list);
      if (task->parent != current_task || (pid != -1 && task->pid != pid))
        continue;

      has_children = 1;

      /* destroy first zombie task */
      if (task->state == TASK_ZOMBIE) {
        if (wstatus != NULL)
          *wstatus = task->exit_code;

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
