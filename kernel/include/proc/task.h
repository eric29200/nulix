#ifndef _TASK_H_
#define _TASK_H_

#include <proc/elf.h>
#include <proc/timer.h>
#include <mm/paging.h>
#include <mm/mmap.h>
#include <fs/fs.h>
#include <ipc/signal.h>
#include <lib/list.h>
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
 * Kernel task structure.
 */
struct task_t {
	pid_t				pid;				/* process id */
	pid_t				pgid;				/* process group id */
	uint8_t				state;				/* process state */
	char				name[TASK_NAME_LEN];		/* process name */
	uid_t				uid;				/* user id */
	uid_t				euid;				/* effective user id */
	uid_t				suid;				/* saved user id */
	gid_t				gid;				/* group id */
	gid_t				egid;				/* effective group id */
	gid_t				sgid;				/* saved group id */
	uint16_t			umask;				/* umask */
	dev_t				tty;				/* attached tty */
	int			 	exit_code;			/* exit code */
	struct task_t *		 	parent;				/* parent process */
	uint32_t			kernel_stack;			/* kernel stack */
	uint32_t			esp;				/* kernel stack pointer */
	uint32_t			user_entry;			/* user entry point */
	uint32_t			user_stack;			/* user stack */
	uint32_t			start_text;			/* user text segment start */
	uint32_t			end_text;			/* user text segment end */
	uint32_t			start_brk;			/* user data segment start */
	uint32_t			end_brk;			/* user data segment end */
	struct list_head_t		vm_list;			/* virtual memory areas */
	void *				waiting_chan;			/* waiting channel */
	uint32_t			timeout;			/* timeout (used by sleep) */
	sigset_t			sigpend;			/* pending signals */
	sigset_t			sigmask;			/* masked signals */
	struct sigaction_t		signals[NSIGS];			/* signal handlers */
	struct inode_t *		cwd;				/* current working directory */
	char				cwd_path[MAX_PATH_LEN];		/* current working directory path */
	struct inode_t *		root;				/* root directory */
	struct registers_t		user_regs;			/* saved registers at syscall entry */
	struct registers_t		signal_regs;			/* saved registers at signal entry */
	struct page_directory_t *	pgd;				/* page directory */
	struct file_t *			 filp[NR_OPEN];			/* opened files */
	struct timer_event_t		sig_tm;				/* signal timer */
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

struct task_t *create_kernel_thread(void (*func)(void *), void *arg);
struct task_t *create_init_task(struct task_t *parent);
struct task_t *fork_task(struct task_t *parent);
void destroy_task(struct task_t *task);
struct task_t *get_task(pid_t pid);

#endif
