#include <sys/syscall.h>
#include <x86/interrupt.h>

/* system calls table */
static const void *syscalls[] = {
  [__NR_exit]       = NULL,
  [__NR_fork]       = NULL,
  [__NR_read]       = sys_read,
  [__NR_write]      = sys_write,
  [__NR_mknod]      = NULL,
  [__NR_open]       = sys_open,
  [__NR_close]      = sys_close,
};

/*
 * System call dispatcher.
 */
static void syscall_handler(struct registers_t *regs)
{
  if (regs->eax < SYSCALLS_NUM && syscalls[regs->eax] != NULL)
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
