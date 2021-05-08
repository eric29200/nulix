#include <sys/syscall.h>

/*
 * Wait 4 system call.
 */
pid_t sys_wait4(int *wstatus, int options, struct rusage_t *rusage)
{
  UNUSED(rusage);
  return sys_waitpid(-1, wstatus, options);
}
