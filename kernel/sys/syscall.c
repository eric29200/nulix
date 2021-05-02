#include <sys/syscall.h>
#include <x86/interrupt.h>
#include <proc/sched.h>
#include <string.h>

/* system calls table */
static const void *syscalls[] = {
  [__NR_exit]       = sys_exit,
  [__NR_fork]       = sys_fork,
  [__NR_read]       = sys_read,
  [__NR_write]      = sys_write,
  [__NR_open]       = sys_open,
  [__NR_close]      = sys_close,
  [__NR_stat]       = sys_stat,
  [__NR_execve]     = sys_execve,
  [__NR_sbrk]       = sys_sbrk,
  [__NR_sleep]      = sys_sleep,
  [__NR_dup]        = sys_dup,
  [__NR_dup2]       = sys_dup2,
  [__NR_wait]       = sys_wait,
  [__NR_access]     = sys_access,
  [__NR_chdir]      = sys_chdir,
};

/*
 * System call dispatcher.
 */
static void syscall_handler(struct registers_t *regs)
{
  /* system call not handled */
  if (regs->eax >= SYSCALLS_NUM || syscalls[regs->eax] == NULL)
    return;

  /* save current registers */
  memcpy(&current_task->user_regs, regs, sizeof(struct registers_t));

  /* execute system call */
  regs->eax = ((syscall_f) syscalls[regs->eax])(regs->ebx, regs->ecx, regs->edx, regs->esi, regs->edi, regs->ebp);
}

/*
 * Init system calls.
 */
void init_syscall()
{
  /* register syscall interrupt handler */
  register_interrupt_handler(0x80, syscall_handler);
}
