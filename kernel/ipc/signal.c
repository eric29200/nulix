#include <ipc/signal.h>
#include <proc/sched.h>
#include <sys/syscall.h>
#include <stderr.h>

#define BLOCKABLE		(~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

extern void return_user_mode(struct registers_t *regs);

/*
 * Change action taken by a process on receipt of a signal.
 */
int do_sigaction(int signum, const struct sigaction_t *act, struct sigaction_t *oldact)
{
	if (signum <= 0 || signum > NSIGS)
		return -EINVAL;

	/* SIGSTOP and SIGKILL actions can't be redefined */
	if (signum == SIGSTOP || signum == SIGKILL)
		return -EINVAL;

	/* save old action */
	if (oldact)
		*oldact = current_task->sig->action[signum - 1];

	/* set new action */
	current_task->sig->action[signum - 1] = *act;

	return 0;
}

/*
 * Change blocked signals.
 */
int do_sigprocmask(int how, const sigset_t *set, sigset_t *oldset)
{
	/* save current sigset */
	if (oldset)
		*oldset = current_task->sigmask;

	if (!set)
		return 0;

	/* update sigmask */
	if (how == SIG_BLOCK)
		current_task->sigmask |= *set;
	else if (how == SIG_UNBLOCK)
		current_task->sigmask &= ~*set;
	else if (how == SIG_SETMASK)
		current_task->sigmask = *set;
	else
		return -EINVAL;

	return 0;
}

/*
 * Signal return .
 */
int do_sigreturn()
{
	/* restore saved registers before signal handler */
	memcpy(&current_task->user_regs, &current_task->signal_regs, sizeof(struct registers_t));

	return 0;
}

/*
 * Signal return trampoline (this code is executed in user mode).
 */
static int sigreturn()
{
	int ret;

	/* call system call register (= go in kernel mode to change interrupt register and go back in previous user code) */
	__asm__ __volatile__("int $0x80" : "=a" (ret) : "0" (__NR_sigreturn));

	return ret;
}

/*
 * Wait for signal.
 */
int do_sigsuspend(sigset_t *newset)
{
	struct registers_t *regs = (struct registers_t *) &newset;

	/* set new sigmask */
	current_task->saved_sigmask = current_task->sigmask;
	sigdelsetmask(newset, ~BLOCKABLE);
	current_task->sigmask = *newset;

	/* wait for signal */
	regs->eax = -EINTR;
	for (;;) {
		current_task->state = TASK_SLEEPING;
		schedule();

		if (do_signal(regs))
			return -EINTR;
	}
}

/*
 * Handle signal of current task.
 */
int do_signal(struct registers_t *regs)
{
	struct sigaction_t *act;
	uint32_t *esp;
	int sig;

	/* get first unblocked signal */
	for (sig = 0; sig < NSIGS; sig++)
		if (sigismember(&current_task->sigpend, sig) && !sigismember(&current_task->sigmask, sig))
			break;

	/* no signal */
	if (sig == NSIGS)
		goto out;

	/* remove signal from current task */
	sigdelset(&current_task->sigpend, sig);
	act = &current_task->sig->action[sig - 1];

	/* ignore signal handler */
	if (act->sa_handler == SIG_IGN)
		goto out;

	/* default signal handler */
	if (act->sa_handler == SIG_DFL) {
		switch (sig) {
			/* ignore those signals */
			case SIGCONT:
			case SIGCHLD:
			case SIGWINCH:
				goto out;
			case SIGSTOP:
			case SIGTSTP:
				current_task->state = TASK_STOPPED;
				current_task->exit_code = sig;
				task_wakeup_all(&current_task->parent->wait_child_exit);
				goto out;
			default:
				sys_exit(sig);
				goto out;
		}
	}

	/* signal in system call : restore user registers */
	if (current_task->in_syscall)
		memcpy(regs, &current_task->user_regs, sizeof(struct registers_t));

	/* save interrupt registers, to restore it at the end of signal */
	memcpy(&current_task->signal_regs, regs, sizeof(struct registers_t));

	/* prepare a stack for signal handler */
	esp = (uint32_t *) regs->useresp;
	*(--esp) = sig;
	*(--esp) = (uint32_t) sigreturn;

	/* changer interrupt registers to return back in signal handler */
	regs->useresp = (uint32_t) esp;
	regs->eip = (uint32_t) act->sa_handler;

out:
	/* restore sigmask */
	if (current_task->saved_sigmask) {
		current_task->sigmask = current_task->saved_sigmask;
		current_task->saved_sigmask = 0;
	}

	/* signal in system call : return to user mode */
	if (current_task->in_syscall) {
		current_task->in_syscall = 0;
		regs->eax = -EINTR;
		return_user_mode(regs);
	}

	return 0;
}