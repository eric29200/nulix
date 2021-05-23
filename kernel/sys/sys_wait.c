#include <sys/syscall.h>
#include <proc/sched.h>
#include <proc/wait.h>
#include <stderr.h>

/*
 * Wait system call.
 */
pid_t sys_waitpid(pid_t pid, int *wstatus, int options)
{
  struct list_head_t *pos;
  struct task_t *task;
  int has_children;
  pid_t child_pid;

  /* unused options flags */
  UNUSED(options);

  /* look for a terminated child */
  for (;;) {
    has_children = 0;

    /* search zombie child */
    list_for_each(pos, &current_task->list) {
      task = list_entry(pos, struct task_t, list);

      /* check task (see man waitpid) */
      if (pid > 0 && task->pid != pid)
        continue;
      else if (pid == 0 && task->pgid != current_task->pgid)
        continue;
      else if (pid != -1 && task->pgid != -pid)
        continue;
      else if (task->parent != current_task)
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

    /* no wait : return */
    if (options & WNOHANG)
      return 0;

    /* else wait for child */
    task_sleep(current_task);
  }
}
