#include <sys/syscall.h>
#include <proc/sched.h>

/*
 * Change data segment end address.
 */
void *sys_brk(void *addr)
{
  /* current brk is asked */
  if ((uint32_t) addr < current_task->end_text)
    return (void *) current_task->end_brk;

  /* update brk */
  current_task->end_brk = (uint32_t) addr;
  return (void *) current_task->end_brk;
}
