#include <sys/syscall.h>

/*
 * Signal return system call.
 */
int sys_sigreturn()
{
  /* restore saved registers before signal handler */
  memcpy(&current_task->user_regs, &current_task->signal_regs, sizeof(struct registers_t));

  return 0;
}
