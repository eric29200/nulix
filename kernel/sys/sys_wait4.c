#include <sys/syscall.h>

/*
 * Wait 4 system call.
 */
pid_t sys_wait4(pid_t pid, int *wstatus, int options, struct rusage_t *rusage)
{
  UNUSED(rusage);
  return sys_waitpid(pid, wstatus, options);
}
