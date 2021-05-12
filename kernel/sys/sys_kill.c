#include <sys/syscall.h>
#include <stderr.h>

/*
 * Kill system call (send a signal to a process).
 */
int sys_kill(pid_t pid, int sig)
{
  /* check signal number (0 is ok : means check permission but do send signal) */
  if (sig < 0 || sig >= NSIGS)
    return -EINVAL;

  /* send signal to process identified by pid */
  if (pid > 0)
    return task_signal(pid, sig);

  /* send signal to all processes in the group */
  if (pid == 0)
    return task_signal_group(pid, sig);

  /* send signal to all processes (except init) */
  return task_signal_all(sig);
}
