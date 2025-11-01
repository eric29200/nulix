#ifndef _TASK_H_
#define _TASK_H_

#include <proc/timer.h>
#include <mm/paging.h>
#include <mm/mmap.h>
#include <fs/fs.h>
#include <ipc/signal.h>
#include <lib/list.h>
#include <x86/tls.h>
#include <x86/segment.h>
#include <resource.h>
#include <stddef.h>

#define STACK_SIZE		0x2000

#define NGROUPS			32

#define TASK_RUNNING		1
#define TASK_SLEEPING		2
#define TASK_STOPPED		3
#define TASK_ZOMBIE		4

#define TASK_NAME_LEN		32

#define NR_OPEN			64
#define MAX_PATH_LEN		1024

/*
 * Task's memory structure.
 */
struct mm_struct {
	int				count;				/* reference counter */
	pgd_t  *			pgd;				/* page directory */
	uint32_t			start_code;			/* user code segment start */
	uint32_t			end_code;			/* user code segment end */
	uint32_t			start_data;			/* user data segment start */
	uint32_t			end_data;			/* user data segment end */
	uint32_t			start_brk;			/* user BRK segment start */
	uint32_t			end_brk;			/* user BRK segment end */
	uint32_t			arg_start;			/* start argument */
	uint32_t			arg_end;			/* end argument */
	uint32_t			env_start;			/* start environ */
	uint32_t			env_end;			/* end environ */
	uint32_t			rss;				/* resident memory */
	struct desc_struct *		ldt;				/* Local Descriptor Table */
	size_t				ldt_size;			/* Local Descriptor Table size */
	uint32_t			swap_address;			/* swap address */
	uint32_t			swap_cnt;			/* swap counter */
	struct list_head		vm_list;			/* virtual memory areas */
};

/*
 * Task's thread structure.
 */
struct thread_struct {
	uint32_t			kernel_stack;				/* kernel stack */
	uint32_t			esp;					/* kernel stack pointer */
	struct desc_struct		tls;					/* Thread Local Storage address */
	struct registers		regs;					/* saved registers at syscall entry */
};

/*
 * Task's file structure.
 */
struct files_struct {
	int				count;				/* reference counter */
	fd_set_t			close_on_exec;			/* close on exec bitmap */
	struct file *			filp[NR_OPEN];			/* opened files */
};

/*
 * Task's file system structure.
 */
struct fs_struct {
	int				count;				/* reference counter */
	uint16_t			umask;				/* umask */
	struct dentry *			pwd;				/* current working directory */
	struct dentry *			root;				/* root directory */
};

/*
 * Task's signal structure.
 */
struct signal_struct {
	int				count;				/* reference counter */
	int				in_sig;				/* task in signal handler ? */
	struct sigaction		action[_NSIG];			/* signal handlers */
};

/*
 * Kernel task structure.
 */
struct task {
	uint32_t			flags;				/* process flags */
	pid_t				pid;				/* process id */
	pid_t				pgrp;				/* process group id */
	pid_t				session;			/* process session id */
	int				leader;				/* 1 if this process is the leader of the session */
	uint8_t				state;				/* process state */
	int				counter;			/* process counter (quantum) */
	int				priority;			/* process priority */
	char				name[TASK_NAME_LEN];		/* process name */
	uid_t				uid;				/* user id */
	uid_t				euid;				/* effective user id */
	uid_t				suid;				/* saved user id */
	uid_t				fsuid;				/* file system uid */
	gid_t				gid;				/* group id */
	gid_t				egid;				/* effective group id */
	gid_t				sgid;				/* saved group id */
	gid_t				fsgid;				/* file system gid */
	size_t				ngroups;			/* number of groups */
	gid_t				groups[NGROUPS];		/* groups */
	int				dumpable;			/* dumpable ? */
	uint32_t			ptrace;				/* ptrace flags */
	siginfo_t *			last_siginfo;			/* last signal informations */
	struct tty *			tty;				/* attached tty */
	time_t				utime;				/* amount of time (in jiffies) that this process has been scheduled in user mode */
	time_t				stime;				/* amount of time (in jiffies) that this process has been scheduled in kernel mode */
	time_t				cutime;				/* amount of time (in jiffies) that this process's waited-for children have been scheduled in user mode */
	time_t				cstime;				/* amount of time (int jiffies) that this process's waited-for children have been scheduled in user mode */
	time_t				start_time;			/* time process started after system boot */
	int			 	exit_code;			/* exit code */
	struct task *		 	parent;				/* parent process */
	struct sigpending		pending;			/* pending signals */
	sigset_t			blocked;			/* blocked signals */
	sigset_t			saved_sigmask;			/* saved signals mask */
	char				in_syscall;			/* process in system call */
	struct rlimit			rlim[RLIM_NLIMITS];		/* resource limits */
	struct registers		signal_regs;			/* saved registers at signal entry */
	struct timer_event		real_timer;			/* timer */
	struct wait_queue *		wait_child_exit;		/* wait queue for child exit */
	struct thread_struct		thread;				/* thread stuff */
	struct fs_struct *		fs;				/* file system stuff */
	struct files_struct *		files;				/* opened files */
	struct mm_struct *		mm;				/* memory regions */
	struct signal_struct *		sig;				/* signals */
	struct semaphore *		vfork_sem;			/* vfork semaphore */
	struct list_head		list;				/* next process */
};

/*
 * Registers structure.
 */
struct task_registers {
	uint32_t 			edi, esi, ebp, esp, ebx, edx, ecx, eax;
	uint32_t 			eip;
	uint32_t 			return_address;
	uint32_t 			parameter1;
	uint32_t 			parameter2;
	uint32_t 			parameter3;
};

/*
 * Binary program.
 */
struct binprm {
	char				buf[128];
	const char *			filename;
	struct dentry *			dentry;
	char *				buf_args;
	char *				p;
	int				argc;
	int				argv_len;
	int				envc;
	int				envp_len;
	uid_t				e_uid;
	gid_t				e_gid;
	int				priv_change;
	int				dumpable;
	int				sh_bang;
};

struct task *create_kinit_task(void (*kinit_func)());
struct task *create_init_task(struct task *parent);
int do_fork(uint32_t clone_flags, uint32_t user_sp);
void destroy_task(struct task *task);
struct task *get_task(pid_t pid);
struct mm_struct *task_alloc_mm();
struct mm_struct *task_dup_mm(struct mm_struct *mm);
void task_exit_signals(struct task *task);
void task_exit_fs(struct task *task);
void task_exit_files(struct task *task);
void task_exit_mm(struct task *task);
void task_exit_mmap(struct mm_struct *mm);
void task_release_mmap(struct task *task);
int task_in_group(struct task *task, gid_t gid);

int search_binary_handler(struct binprm *bprm);
void copy_strings(struct binprm *bprm, int argc, char **argv);
int prepare_binprm(struct binprm *bprm);
int flush_old_exec(struct binprm *bprm);
void compute_creds(struct binprm *bprm);
int open_dentry(struct dentry *dentry, mode_t mode);
int read_exec(struct dentry *dentry, off_t offset, char *addr, size_t count);

pid_t sys_fork();
pid_t sys_vfork();
int sys_clone(uint32_t flags, uint32_t newsp);
int sys_execve(const char *path, char *const argv[], char *const envp[]);
void do_exit(int error_code);
void sys_exit(int status);
void sys_exit_group(int status);
pid_t sys_waitpid(pid_t pid, int *wstatus, int options);
pid_t sys_wait4(pid_t pid, int *wstatus, int options, struct rusage *rusage);

#endif
