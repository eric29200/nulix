#include <sys/syscall.h>
#include <proc/sched.h>

/*
 * Change data segment size.
 */
void *sys_sbrk(size_t n)
{
  void *brk;

  /* update brk */
  brk = (void *) current_task->end_brk;
  current_task->end_brk += n;

  return brk;
}
