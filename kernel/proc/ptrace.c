#include <proc/ptrace.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>

#define EBX				0
#define ECX				1
#define EDX				2
#define ESI				3
#define EDI				4
#define EBP				5
#define EAX				6
#define DS				7
#define ES				8
#define FS				9
#define GS				10
#define ORIG_EAX			11
#define EIP				12
#define CS 				13
#define EFL				14
#define UESP				15
#define SS				16
#define NR_REGS				17

struct user_regs_struct {
	unsigned long			bx;
	unsigned long			cx;
	unsigned long			dx;
	unsigned long			si;
	unsigned long			di;
	unsigned long			bp;
	unsigned long			ax;
	unsigned long			ds;
	unsigned long			es;
	unsigned long			fs;
	unsigned long			gs;
	unsigned long			orig_ax;
	unsigned long			ip;
	unsigned long			cs;
	unsigned long			flags;
	unsigned long			sp;
	unsigned long			ss;
};

struct user_i387_struct {
	long				cwd;
	long				swd;
	long				twd;
	long				fip;
	long				fcs;
	long				foo;
	long				fos;
	long				st_space[20];
};

struct user {
	struct user_regs_struct		regs;
	int				u_fpvalid;
	struct user_i387_struct		i387;
	unsigned long int		u_tsize;
	unsigned long int		u_dsize;
	unsigned long int		u_ssize;
	unsigned long			start_code;
	unsigned long			start_stack;
	unsigned long			u_ar0;
	struct user_i387_struct *	u_fpstate;
	unsigned long			magic;
	char				u_comm[32];
	int				u_debugreg[8];
};

/*
 * Get a register value.
 */
static uint32_t getreg(struct task *child, int regno)
{
	regno >>= 2;

	switch (regno) {
		case FS:
			return child->thread.regs.fs & 0xFFFF;
		case GS:
			return child->thread.regs.gs & 0xFFFF;
		case DS:
			return child->thread.regs.ds & 0xFFFF;
		case ES:
			return child->thread.regs.es & 0xFFFF;
		case SS:
			return child->thread.regs.ss & 0xFFFF;
		case CS:
			return child->thread.regs.cs & 0xFFFF;
		case EIP:
			return child->thread.regs.eip;
		case UESP:
			return child->thread.regs.useresp;
		case EAX:
			return child->thread.regs.eax;
		case EBX:
			return child->thread.regs.ebx;
		case ECX:
			return child->thread.regs.ecx;
		case EDX:
			return child->thread.regs.edx;
		case ESI:
			return child->thread.regs.esi;
		case EDI:
			return child->thread.regs.edi;
		case EBP:
			return child->thread.regs.ebp;
		case EFL:
			return child->thread.regs.eflags;
		case ORIG_EAX:
			return child->thread.regs.orig_eax;
		default:
			printf("getreg: bad register in getreg() : %d", regno);
			break;
	}

	return 0;
}

/*
 * Trace current process.
 */
static int ptrace_traceme()
{
	/* already traced */
	if (current_task->ptrace & PT_PTRACED)
		return -EPERM;

	/* set traced */
	current_task->ptrace |= PT_PTRACED;
	return 0;
}

/*
 * Attach a process.
 */
static int ptrace_attach(struct task *child, long request)
{
	/* can't attach current task */
	if (child == current_task)
		return -EPERM;

	/* check permissions */
	if (!child->dumpable
		|| current_task->uid != child->euid
		|| current_task->uid != child->suid
		|| current_task->uid != child->uid
		|| current_task->gid != child->egid
		|| current_task->gid != child->sgid
		|| current_task->gid != child->gid)
		return -EPERM;

	/* same process can't be attached many times */
	if (child->ptrace & PT_PTRACED)
		return -EPERM;

	/* mark process traced */
	child->ptrace |= PT_PTRACED;

	/* adopt child */
	if (child->parent != current_task)
		child->parent = current_task;

	/* stop child */
	if (request != PTRACE_SEIZE)
		send_sig(child, SIGSTOP, 1);

	return 0;
}

/*
 * Set options.
 */
static int ptrace_setoptions(struct task *child, uint32_t data)
{
	if (data & ~(unsigned long) PTRACE_O_MASK)
		return -EINVAL;

	child->ptrace &= ~(PTRACE_O_MASK << PT_OPT_FLAG_SHIFT);
	child->ptrace |= (data << PT_OPT_FLAG_SHIFT);

	return 0;
}

/*
 * Get last signal informations.
 */
static int ptrace_getsiginfo(struct task *child, siginfo_t *info)
{
	if (!child->last_siginfo)
		return -ESRCH;

	memcpy(info, child->last_siginfo, sizeof(siginfo_t));
	return 0;
}

/*
 * Ptrace system call.
 */
int sys_ptrace(long request, pid_t pid, uint32_t addr, uint32_t data)
{
	uint32_t tmp = 0, i;
	struct task *child;

	/* trace current process */
	if (request == PTRACE_TRACEME)
		return ptrace_traceme();

	/* find task */
	child = find_task(pid);
	if (!child)
		return -ESRCH;

	/* init process can't be traced */
	if (pid == 1)
		return -EPERM;

	/* attach a process */
	if (request == PTRACE_ATTACH || request == PTRACE_SEIZE)
		return ptrace_attach(child, request);

	/* check process */
	if (!(child->ptrace & PT_PTRACED))
		return -ESRCH;
	if (child->state != TASK_STOPPED && request != PTRACE_KILL)
		return -ESRCH;
	if (child->parent != current_task)
		return -ESRCH;

	/* handle request */
	switch (request) {
		case PTRACE_PEEKUSR:
			/* check address */
			if ((addr & (sizeof(data) - 1)) || addr >= sizeof(struct user))
				return -EIO;

			/* get a register value */
			if (addr < sizeof(struct user_regs_struct)) {
				tmp = getreg(child, addr);
			} else {
				printf("ptrace: can't peek at adress 0x%x\n", addr);
				return -EIO;
			}

			*((uint32_t *) data) = tmp;
			break;
		case PTRACE_GETREGS:
			/* get registers */
			for (i = 0; i < NR_REGS * sizeof(uint32_t); i += sizeof(uint32_t)) {
				*((uint32_t *) data) = getreg(child, i);
				data += sizeof(uint32_t);
			}

			break;
		case PTRACE_SYSCALL:
			/* check signal */
			if (data > _NSIG)
				return -EIO;

			/* trace system calls */
			child->ptrace |= PT_TRACESYS;

			/* wake up process */
			child->exit_code = data;
			wake_up_process(child);
			break;
		case PTRACE_CONT:
			/* check signal */
			if (data > _NSIG)
				return -EIO;

			/* don't trace system calls */
			child->ptrace &= ~PT_TRACESYS;

			/* wake up process */
			child->exit_code = data;
			wake_up_process(child);
			break;
		case PTRACE_GETSIGINFO:
			return ptrace_getsiginfo(child, (siginfo_t *) data);
		case PTRACE_SETOPTIONS:
			return ptrace_setoptions(child, data);
		default:
			printf("ptrace: unknown request 0x%x\n", request);
			return -EIO;
	}

	return 0;
}

/*
 * Trace a system call.
 */
void syscall_trace()
{
	/* don't trace system calls */
	if ((current_task->ptrace & (PT_PTRACED | PT_TRACESYS)) != (PT_PTRACED | PT_TRACESYS))
		return;

	/* stop task and notify parent */
	current_task->exit_code = SIGTRAP;
	current_task->state = TASK_STOPPED;
	notify_parent(current_task, SIGCHLD);
	schedule();

	/* continue */
	if (current_task->exit_code) {
		send_sig(current_task, current_task->exit_code, 1);
		current_task->exit_code = 0;
	}
}
