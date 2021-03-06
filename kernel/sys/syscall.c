#include <sys/syscall.h>
#include <x86/interrupt.h>
#include <proc/sched.h>
#include <string.h>
#include <stdio.h>

/* system calls table */
static const void *syscalls[] = {
	[__NR_exit]			= sys_exit,
	[__NR_fork]			= sys_fork,
	[__NR_vfork]			= sys_vfork,
	[__NR_read]			= sys_read,
	[__NR_write]			= sys_write,
	[__NR_open]			= sys_open,
	[__NR_close]			= sys_close,
	[__NR_execve]			= sys_execve,
	[__NR_brk]			= sys_brk,
	[__NR_dup]			= sys_dup,
	[__NR_dup2]			= sys_dup2,
	[__NR_getppid]			= sys_getppid,
	[__NR_waitpid]			= sys_waitpid,
	[__NR_access]			= sys_access,
	[__NR_chdir]			= sys_chdir,
	[__NR_mkdir]			= sys_mkdir,
	[__NR_lseek]			= sys_lseek,
	[__NR_getpid]			= sys_getpid,
	[__NR_creat]			= sys_creat,
	[__NR_link]			= sys_link,
	[__NR_unlink]			= sys_unlink,
	[__NR_rmdir]			= sys_rmdir,
	[__NR_wait4]			= sys_wait4,
	[__NR_readv]			= sys_readv,
	[__NR_writev]			= sys_writev,
	[__NR_nanosleep]		= sys_nanosleep,
	[__NR_getdents64]		= sys_getdents64,
	[__NR_getpgid]			= sys_getpgid,
	[__NR_setpgid]			= sys_setpgid,
	[__NR_sigaction]		= sys_sigaction,
	[__NR_rt_sigaction]		= sys_sigaction,
	[__NR_kill]			= sys_kill,
	[__NR_sigprocmask]		= sys_sigprocmask,
	[__NR_rt_sigprocmask]	 	= sys_rt_sigprocmask,
	[__NR_sigreturn]		= sys_sigreturn,
	[__NR_getuid]			= sys_getuid,
	[__NR_geteuid]			= sys_geteuid,
	[__NR_setuid]			= sys_setuid,
	[__NR_getgid]			= sys_getgid,
	[__NR_getegid]			= sys_getegid,
	[__NR_setgid]			= sys_getgid,
	[__NR_getuid32]			= sys_getuid,
	[__NR_geteuid32]		= sys_geteuid,
	[__NR_setuid32]			= sys_setuid,
	[__NR_getgid32]			= sys_getgid,
	[__NR_getegid32]		= sys_getegid,
	[__NR_setgid32]			= sys_getgid,
	[__NR_getcwd]			= sys_getcwd,
	[__NR_ioctl]			= sys_ioctl,
	[__NR_fcntl]			= sys_fcntl,
	[__NR_fcntl64]			= sys_fcntl,
	[__NR_statx]			= sys_statx,
	[__NR_umask]			= sys_umask,
	[__NR_mmap]			= sys_mmap,
	[__NR_mmap2]			= sys_mmap2,
	[__NR_munmap]			= sys_munmap,
	[__NR_modify_ldt]		= sys_modify_ldt,
	[__NR_gettid]			= sys_gettid,
	[__NR_get_thread_area]		= sys_get_thread_area,
	[__NR_set_thread_area]		= sys_set_thread_area,
	[__NR_exit_group]		= sys_exit_group,
	[__NR_set_tid_address]		= sys_set_tid_address,
	[__NR_faccessat]		= sys_faccessat,
	[__NR_faccessat2]		= sys_faccessat2,
	[__NR_mkdirat]			= sys_mkdirat,
	[__NR_linkat]			= sys_linkat,
	[__NR_unlinkat]			= sys_unlinkat,
	[__NR_openat]			= sys_openat,
	[__NR_symlink]			= sys_symlink,
	[__NR_symlinkat]		= sys_symlinkat,
	[__NR_readlink]			= sys_readlink,
	[__NR_readlinkat]		= sys_readlinkat,
	[__NR_uname]			= sys_uname,
	[__NR_pipe]			= sys_pipe,
	[__NR_clock_gettime64]		= sys_clock_gettime64,
	[__NR_sysinfo]			= sys_sysinfo,
	[__NR_rename]			= sys_rename,
	[__NR_renameat]			= sys_renameat,
	[__NR_renameat2]		= sys_renameat2,
	[__NR_socket]			= sys_socket,
	[__NR_sendto]			= sys_sendto,
	[__NR_recvmsg]			= sys_recvmsg,
	[__NR_poll]			= sys_poll,
	[__NR_bind]			= sys_bind,
	[__NR_recvfrom]			= sys_recvfrom,
	[__NR_setitimer]		= sys_setitimer,
	[__NR_connect]			= sys_connect,
	[__NR_llseek]			= sys_llseek,
	[__NR_chmod]			= sys_chmod,
	[__NR_listen]			= sys_listen,
	[__NR_select]			= sys_select,
	[__NR_accept]			= sys_accept,
	[__NR_mknod]			= sys_mknod,
	[__NR_getsockname]		= sys_getsockname,
	[__NR_getpeername]		= sys_getpeername,
	[__NR_chown32]			= sys_chown,
	[__NR_fchown32]			= sys_fchown,
	[__NR_fchmod]			= sys_fchmod,
	[__NR_setgroups32]		= sys_setgroups,
	[__NR_truncate64]		= sys_truncate64,
	[__NR_ftruncate64]		= sys_ftruncate64,
	[__NR_utimensat]		= sys_utimensat,
	[__NR_sync]			= sys_sync,
	[__NR_chroot]			= sys_chroot,
	[__NR_reboot]			= sys_reboot,
	[__NR_mount]			= sys_mount,
	[__NR_statfs64]			= sys_statfs64,
	[__NR_fstatfs64]		= sys_fstatfs64,
	[__NR_pselect6]			= sys_pselect6,
	[__NR_fchmodat]			= sys_fchmodat,
	[__NR_fchownat]			= sys_fchownat,
	[__NR_copy_file_range]		= sys_copy_file_range,
	[__NR_stat64]			= sys_stat64,
	[__NR_lstat64]			= sys_lstat64,
	[__NR_fstat64]			= sys_fstat64,
	[__NR_fstatat64]		= sys_fstatat64,
};

/*
 * System call dispatcher.
 */
static void syscall_handler(struct registers_t *regs)
{
	int ret;

	/* system call not handled */
	if (regs->eax >= SYSCALLS_NUM || syscalls[regs->eax] == NULL) {
		printf("Unknown system call : %d\n", regs->eax);
		return;
	}

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

