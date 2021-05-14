#include <sys/syscall.h>
#include <x86/interrupt.h>
#include <proc/sched.h>
#include <string.h>

/* system calls table */
static const void *syscalls[] = {
  [__NR_exit]             = sys_exit,
  [__NR_fork]             = sys_fork,
  [__NR_read]             = sys_read,
  [__NR_write]            = sys_write,
  [__NR_open]             = sys_open,
  [__NR_close]            = sys_close,
  [__NR_stat]             = sys_stat,
  [__NR_execve]           = sys_execve,
  [__NR_brk]              = sys_brk,
  [__NR_dup]              = sys_dup,
  [__NR_dup2]             = sys_dup2,
  [__NR_getppid]          = sys_getppid,
  [__NR_waitpid]          = sys_waitpid,
  [__NR_access]           = sys_access,
  [__NR_chdir]            = sys_chdir,
  [__NR_mkdir]            = sys_mkdir,
  [__NR_lseek]            = sys_lseek,
  [__NR_getpid]           = sys_getpid,
  [__NR_creat]            = sys_creat,
  [__NR_unlink]           = sys_unlink,
  [__NR_rmdir]            = sys_rmdir,
  [__NR_wait4]            = sys_wait4,
  [__NR_getdents]         = sys_getdents,
  [__NR_readv]            = sys_readv,
  [__NR_writev]           = sys_writev,
  [__NR_nanosleep]        = sys_nanosleep,
  [__NR_getdents64]       = sys_getdents64,
  [__NR_getpgid]          = sys_getpgid,
  [__NR_setpgid]          = sys_setpgid,
  [__NR_sigaction]        = sys_sigaction,
  [__NR_rt_sigaction]     = sys_sigaction,
  [__NR_kill]             = sys_kill,
  [__NR_sigprocmask]      = sys_sigprocmask,
  [__NR_rt_sigprocmask]   = sys_rt_sigprocmask,
  [__NR_sigreturn]        = sys_sigreturn,
  [__NR_getuid]           = sys_getuid,
  [__NR_geteuid]          = sys_geteuid,
  [__NR_setuid]           = sys_setuid,
  [__NR_getgid]           = sys_getgid,
  [__NR_getegid]          = sys_getegid,
  [__NR_setgid]           = sys_getgid,
  [__NR_getuid32]         = sys_getuid,
  [__NR_geteuid32]        = sys_geteuid,
  [__NR_setuid32]         = sys_setuid,
  [__NR_getgid32]         = sys_getgid,
  [__NR_getegid32]        = sys_getegid,
  [__NR_setgid32]         = sys_getgid,
  [__NR_getcwd]           = sys_getcwd,
  [__NR_ioctl]            = sys_ioctl,
  [__NR_fcntl]            = sys_fcntl,
  [__NR_fcntl64]          = sys_fcntl,
};

/*
 * System call dispatcher.
 */
static void syscall_handler(struct registers_t *regs)
{
  uint32_t ret;

  /* system call not handled */
  if (regs->eax >= SYSCALLS_NUM || syscalls[regs->eax] == NULL)
    return;

  /* save current registers */
  memcpy(&current_task->user_regs, regs, sizeof(struct registers_t));

  /* execute system call */
  ret = ((syscall_f) syscalls[regs->eax])(regs->ebx, regs->ecx, regs->edx, regs->esi, regs->edi, regs->ebp);

  /* restore registers and set return value */
  memcpy(regs, &current_task->user_regs, sizeof(struct registers_t));
  regs->eax = ret;
}

/*
 * Init system calls.
 */
void init_syscall()
{
  /* register syscall interrupt handler */
  register_interrupt_handler(0x80, syscall_handler);
}
