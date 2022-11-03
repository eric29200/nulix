#ifndef _TASK_H_
#define _TASK_H_

#include <proc/timer.h>
#include <mm/paging.h>
#include <mm/mmap.h>
#include <fs/fs.h>
#include <ipc/signal.h>
#include <lib/list.h>
#include <x86/tls.h>
#include <stddef.h>

#define STACK_SIZE		0x2000

#define TASK_RUNNING		1
#define TASK_SLEEPING		2
#define TASK_STOPPED		3
#define TASK_ZOMBIE		4

#define TASK_NAME_LEN		32

#define NR_OPEN			32
#define MAX_PATH_LEN		1024

/*
 * Task's memory structure.
 */
struct mm_struct {
	int				count;				/* reference counter */
	struct page_directory_t *	pgd;				/* page directory */
	uint32_t			start_text;			/* user text segment start */
	uint32_t			end_text;			/* user text segment end */
	uint32_t			start_brk;			/* user data segment start */
	uint32_t			end_brk;			/* user data segment end */
	uint32_t			arg_start;			/* start argument */
	uint32_t			arg_end;			/* end argument */
	uint32_t			env_start;			/* start environ */
	uint32_t			env_end;			/* end environ */
	struct list_head_t		vm_list;			/* virtual memory areas */
};

/*
 * Task's file structure.
 */
struct files_struct {
	int				count;				/* reference counter */
	fd_set_t			close_on_exec;			/* close on exec bitmap */
	struct file_t *			filp[NR_OPEN];			/* opened files */
};

/*
 * Task's file system structure.
 */
struct fs_struct {
	int				count;				/* reference counter */
	uint16_t			umask;				/* umask */
	struct inode_t *		cwd;				/* current working directory */
	struct inode_t *		root;				/* root directory */
};

/*
 * Task's signal structure.
 */
struct signal_struct {
	int				count;				/* reference counter */
	struct sigaction_t		action[NSIGS];			/* signal handlers */
};

/*
 * Kernel task structure.
 */
struct task_t {
	uint32_t			flags;				/* process flags */
	pid_t				pid;				/* process id */
	pid_t				pgrp;				/* process group id */
	pid_t				session;			/* process session id */
	int				leader;				/* 1 if this process is the leader of the session */
	uint8_t				state;				/* process state */
	char				name[TASK_NAME_LEN];		/* process name */
	uid_t				uid;				/* user id */
	uid_t				euid;				/* effective user id */
	uid_t				suid;				/* saved user id */
	gid_t				gid;				/* group id */
	gid_t				egid;				/* effective group id */
	gid_t				sgid;				/* saved group id */
	dev_t				tty;				/* attached tty */
	uint32_t			utime;				/* amount of time that this process has been scheduled in user mode */
	uint32_t			stime;				/* amount of time that this process has been scheduled in kernel mode */
	uint32_t			cutime;				/* amount of time that this process's waited-for children have been scheduled in user mode */
	uint32_t			cstime;				/* amount of time that this process's waited-for children have been scheduled in user mode */
	uint32_t			start_time;			/* time process started after system boot */
	int			 	exit_code;			/* exit code */
	struct task_t *		 	parent;				/* parent process */
	uint32_t			kernel_stack;			/* kernel stack */
	uint32_t			esp;				/* kernel stack pointer */
	uint32_t			timeout;			/* timeout (used by sleep) */
	sigset_t			sigpend;			/* pending signals */
	sigset_t			sigmask;			/* masked signals */
	struct registers_t		user_regs;			/* saved registers at syscall entry */
	struct registers_t		signal_regs;			/* saved registers at signal entry */
	struct user_desc_t		tls;				/* Thread Local Storage address */
	struct timer_event_t		sig_tm;				/* signal timer */
	struct wait_queue_t *		wait_child_exit;		/* wait queue for child exit */
	struct fs_struct *		fs;				/* file system stuff */
	struct files_struct *		files;				/* opened files */
	struct mm_struct *		mm;				/* memory regions */
	struct signal_struct *		sig;				/* signals */
	struct semaphore_t *		vfork_sem;			/* vfork semaphore */
	struct list_head_t		list;				/* next process */
};

/*
 * Registers structure.
 */
struct task_registers_t {
	uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
	uint32_t eip;
	uint32_t return_address;
	uint32_t parameter1;
	uint32_t parameter2;
	uint32_t parameter3;
};

/*
 * Binary arguments.
 */
struct binargs_t {
	char	*buf;
	int	argc;
	int	argv_len;
	int	envc;
	int	envp_len;
};

struct task_t *create_kernel_thread(void (*func)(void *), void *arg);
struct task_t *create_init_task(struct task_t *parent);
int do_fork(uint32_t clone_flags, uint32_t user_sp);
void destroy_task(struct task_t *task);
struct task_t *get_task(pid_t pid);
struct mm_struct *task_dup_mm(struct mm_struct *mm);
void task_exit_signals(struct task_t *task);
void task_exit_fs(struct task_t *task);
void task_exit_files(struct task_t *task);
void task_exit_mmap(struct mm_struct *mm);
void task_release_mmap(struct task_t *task);

#endif
