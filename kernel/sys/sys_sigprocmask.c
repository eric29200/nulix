#include <sys/syscall.h>
#include <stderr.h>

/*
 * Change blocked signals system call.
 */
int sys_sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
  /* save current sigset */
  if (oldset)
    *oldset = current_task->sigmask;

  if (!set)
    return 0;

  /* update sigmask */
  if (how == SIG_BLOCK)
    current_task->sigmask |= *set;
  else if (how == SIG_UNBLOCK)
    current_task->sigmask &= ~*set;
  else if (how == SIG_SETMASK)
    current_task->sigmask = *set;
  else
    return -EINVAL;

  return 0;
}
