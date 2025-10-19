#include <ipc/signal.h>
#include <proc/sched.h>
#include <proc/ptrace.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stderr.h>

#define BLOCKABLE		(~(sigmask(SIGKILL) | sigmask(SIGSTOP)))

extern void return_user_mode(struct registers *regs);

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
 * Handle user signal handler.
 */
static void handle_signal(struct registers *regs, int sig, struct sigaction *act)
{
	uint32_t *esp;

	/* signal in system call : restore user registers */
	if (current_task->in_syscall)
		memcpy(regs, &current_task->thread.regs, sizeof(struct registers));

	/* save interrupt registers, to restore it at the end of signal */
	memcpy(&current_task->signal_regs, regs, sizeof(struct registers));

	/* prepare a stack for signal handler */
	esp = (uint32_t *) regs->useresp;
	*(--esp) = sig;
	*(--esp) = (uint32_t) sigreturn;

	/* changer interrupt registers to return back in signal handler */
	regs->useresp = (uint32_t) esp;
	regs->eip = (uint32_t) act->sa_handler;

	/* restore sigmask */
	if (current_task->saved_sigmask) {
		current_task->blocked = current_task->saved_sigmask;
		current_task->saved_sigmask = 0;
	}

	/* signal in system call : return to user mode */
	if (current_task->in_syscall) {
		current_task->in_syscall = 0;
		regs->eax = -EINTR;
		return_user_mode(regs);
	}
}

/*
 * Handle signal of current task.
 */
int do_signal(struct registers *regs)
{
	struct sigaction *act;
	int sig;

	/* get first unblocked signal */
	for (sig = 0; sig < NSIGS; sig++)
		if (sigismember(&current_task->signal, sig) && !sigismember(&current_task->blocked, sig))
			break;

	/* no signal */
	if (sig == NSIGS)
		goto out;

	/* remove signal from current task */
	sigdelset(&current_task->signal, sig);
	act = &current_task->sig->action[sig - 1];

	/* traced process */
	if ((current_task->ptrace & PT_PTRACED) && sig != SIGKILL) {
			/* let the debugger run */
			current_task->exit_code = sig;
			current_task->state = TASK_STOPPED;
			__task_signal(current_task->parent, SIGCHLD);
			wake_up(&current_task->parent->wait_child_exit);
			schedule();

			/* did the debugger cancel the sig ? */
			sig = current_task->exit_code;
			if (!sig)
				goto out;
			current_task->exit_code = 0;

			/* ingore SIGSTOP */
			if (sig == SIGSTOP)
				goto out;

			/* if the (new) signal is now blocked, requeue it */
			if (sigismember(&current_task->blocked, sig)) {
				__task_signal(current_task, sig);
				goto out;
			}
		}

	/* ignore signal handler */
	if (act->sa_handler == SIG_IGN)
		goto out;

	/* default signal handler */
	if (act->sa_handler == SIG_DFL) {
		/* init task gets no signals it doesn't want */
		if (current_task->pid == 1)
			goto out;

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
				__task_signal(current_task->parent, SIGCHLD);
				wake_up(&current_task->parent->wait_child_exit);
				schedule();
				goto out;
			default:
				do_exit(sig);
				goto out;
		}
	}

	handle_signal(regs, sig, act);
	return 1;
out:
	/* restore sigmask */
	if (current_task->saved_sigmask) {
		current_task->blocked = current_task->saved_sigmask;
		current_task->saved_sigmask = 0;
	}

	/* signal in system call : return to user mode */
	if (current_task->in_syscall) {
		current_task->in_syscall = 0;
		regs->eax = -EINTR;
		return_user_mode(regs);
	}

	/* interrupted system call : redo it */
	if (regs->orig_eax && (int) regs->eax == -EINTR) {
		regs->eax = regs->orig_eax;
		regs->eip -= 2;
	}

	return 0;
}

/*
 * Wait for a signal system call.
 */
int sys_rt_sigsuspend(sigset_t *newset, size_t sigsetsize)
{
	struct registers *regs = (struct registers *) &newset;

	UNUSED(sigsetsize);

	/* set new sigmask */
	current_task->saved_sigmask = current_task->blocked;
	current_task->blocked = *newset & BLOCKABLE;

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
 * Examine pending signals.
 */
int sys_rt_sigpending(sigset_t *set, size_t sigsetsize)
{
	UNUSED(sigsetsize);

	if (!set)
		return -EINVAL;

	*set = current_task->signal;

	return 0;
}

/*
 * Sigaction system call (= change action taken by a process on receipt of a specific signal).
 */
int sys_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
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
 * Change blocked signals system call.
 */
int sys_rt_sigprocmask(int how, const sigset_t *set, sigset_t *oldset, size_t sigsetsize)
{
	UNUSED(sigsetsize);

	/* save current sigset */
	if (oldset)
		*oldset = current_task->blocked;

	if (!set)
		return 0;

	/* update sigmask */
	if (how == SIG_BLOCK)
		current_task->blocked |= *set;
	else if (how == SIG_UNBLOCK)
		current_task->blocked &= ~*set;
	else if (how == SIG_SETMASK)
		current_task->blocked = *set;
	else
		return -EINVAL;

	return 0;
}

/*
 * Signal return system call.
 */
int sys_sigreturn()
{
	/* restore saved registers before signal handler */
	memcpy(&current_task->thread.regs, &current_task->signal_regs, sizeof(struct registers));

	/* return value of syscall interrupted by signal */
	return current_task->signal_regs.eax;
}

/*
 * Kill system call (send a signal to a process).
 */
int sys_kill(pid_t pid, int sig)
{
	/* check signal number (0 is ok : means check permission but do not send signal) */
	if (sig < 0 || sig >= NSIGS)
		return -EINVAL;

	/* send signal to process identified by pid */
	if (pid > 0)
		return task_signal(pid, sig);

	/* send signal to all processes in the group of current task */
	if (pid == 0)
		return task_signal_group(current_task->pgrp, sig);

	/* send signal to all processes (except init) */
	if (pid == -1)
		task_signal_all(sig);

	/* signal to all processes in the group -pid */
	return task_signal_group(-pid, sig);
}

/*
 * Thread kill system call (send a signal to a process).
 */
int sys_tkill(pid_t pid, int sig)
{
	/* only valid for single tasks */
	if (pid <= 0)
		return -EINVAL;

	return sys_kill(pid, sig);
}

/*
 * Wait for a signal.
 */
int sys_rt_sigtimedwait(const sigset_t *usigset, void *info, const struct timespec *uts, size_t sigsetsize)
{
	UNUSED(usigset);
	UNUSED(info);
	UNUSED(uts);
	UNUSED(sigsetsize);
	return 0;
}
