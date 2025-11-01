#include <proc/sched.h>
#include <ipc/signal.h>
#include <proc/ptrace.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stderr.h>

#define MAX_QUEUED_SIGNALS	1024

extern void return_user_mode(struct registers *regs);

/* global variables */
static size_t nr_queued_signals = 0;

/*
 * Initialize pending signals.
 */
void init_sigpending(struct sigpending *pending)
{
	sigemptyset(&pending->signal);
	INIT_LIST_HEAD(&pending->list);
}

/*
 * Flush signals.
 */
void flush_signals(struct task *task)
{
	struct sigpending *queue = &task->pending;
	struct sigqueue *q;

	/* reset sigset */
	sigemptyset(&queue->signal);

	/* clear queue */
	while (!list_empty(&queue->list)) {
		q = list_entry(queue->list.next, struct sigqueue, list);
		list_del(&q->list);
		kfree(q);
	}
}

/*
 * Send a signal.
 */
static int send_signal(struct sigpending *pending, int sig, siginfo_t *info)
{
	struct sigqueue *q = NULL;

	/* allocate a new signal */
	if (nr_queued_signals < MAX_QUEUED_SIGNALS)
		q = (struct sigqueue *) kmalloc(sizeof(struct sigqueue));

	/* queue signal */
	if (q) {
		nr_queued_signals++;
		list_add_tail(&q->list, &pending->list);

		switch ((uint32_t) info) {
			case 0:
				info->si_signo = sig;
				info->si_errno = 0;
				info->si_code = SI_USER;
				info->__si_fields.__si_common.__first.__piduid.si_pid = current_task->pid;
				info->__si_fields.__si_common.__first.__piduid.si_uid = current_task->uid;
				break;
			case 1:
				info->si_signo = sig;
				info->si_errno = 0;
				info->si_code = SI_KERNEL;
				info->__si_fields.__si_common.__first.__piduid.si_pid = 0;
				info->__si_fields.__si_common.__first.__piduid.si_uid = 0;
				break;
			default:
				memcpy(&q->info, info, sizeof(siginfo_t));
				break;
		}
	}

	/* add to pending signals */
	sigaddset(&pending->signal, sig);

	return 0;
}

/*
 * Deliver a signal.
 */
static int deliver_signal(struct task *task, int sig, siginfo_t *info)
{
	int ret;

	/* send signal */
	ret = send_signal(&task->pending, sig, info);
	if (ret)
		return ret;

	/* wake up process if sleeping */
	if (!sigismember(&task->blocked, sig))
		if (task->state == TASK_SLEEPING || task->state == TASK_STOPPED)
			wake_up_process(task);

	return 0;
}

/*
 * Send a signal to a task.
 */
int send_sig_info(struct task *task, int sig, siginfo_t *info)
{
	/* check signal */
	if (sig < 0 || sig > _NSIG)
		return -EINVAL;
	if (!sig || !task->sig)
		return 0;

	/* queue only one non RT signal */
	if (sig < SIGRTMIN && sigismember(&task->pending.signal, sig))
		return 0;

	/* deliver signal */
	return deliver_signal(task, sig, info);
}

/*
 * Send a signal to a task.
 */
int send_sig(struct task *task, int sig, int priv)
{
	return send_sig_info(task, sig, (void *) (priv != 0));
}

/*
 * Send a signal to a task.
 */
int kill_proc_info(pid_t pid, int sig, siginfo_t *info)
{
	struct task *task;

	/* get task */
	task = get_task(pid);
	if (!task)
		return -ESRCH;

	/* send signal */
	return send_sig_info(task, sig, info);
}

/*
 * Send a signal to a task.
 */
int kill_proc(pid_t pid, int sig, int priv)
{
	return kill_proc_info(pid, sig, (void *) (priv != 0));
}

/*
 * Send a signal to all tasks in a group.
 */
int kill_pg_info(pid_t pgrp, int sig, siginfo_t *info)
{
	int ret = -ESRCH, err, found = 0;
	struct list_head *pos;
	struct task *task;

	/* check group */
	if (pgrp <= 0)
		return -EINVAL;

	list_for_each(pos, &tasks_list) {
		task = list_entry(pos, struct task, list);
		if (task->pgrp == pgrp) {
			err = send_sig_info(task, sig, info);
			if (err)
				ret = err;
			else
				found++;
		}
	}

	if (found)
		ret = 0;

	return ret;
}

/*
 * Send a signal to all tasks in a group.
 */
int kill_pg(pid_t pgrp, int sig, int priv)
{
	return kill_pg_info(pgrp, sig, (void *) (priv != 0));
}

/*
 * Kill something.
 */
static int kill_something_info(pid_t pid, int sig, siginfo_t *info)
{
	int err, ret = 0, count = 0;
	struct list_head *pos;
	struct task *task;

	/* check signal number (0 is ok : means check permission but do not send signal) */
	if (sig < 0 || sig >= _NSIG)
		return -EINVAL;

	/* send signal to all processes in the group of current task */
	if (pid == 0)
		return kill_pg_info(current_task->pgrp, sig, info);

	/* send signal to all processes (except init) */
	if (pid == -1) {
		list_for_each(pos, &tasks_list) {
			task = list_entry(pos, struct task, list);
			if (task->pid > 1 && task != current_task) {
				err = send_sig_info(task, sig, info);
				count++;
				if (err != -EPERM)
					ret = err;
			}
		}

		return count ? ret : -ESRCH;
	}

	/* signal to all processes in the group -pid */
	if (pid < 0)
		return kill_pg_info(-pid, sig, info);

	/* send signal to process identified by pid */
	return kill_proc_info(pid, sig, info);
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
	current_task->sig->in_sig = 1;

	/* prepare a stack for signal handler */
	esp = (uint32_t *) regs->useresp;
	*(--esp) = sig;
	*(--esp) = (uint32_t) sigreturn;

	/* changer interrupt registers to return back in signal handler */
	regs->useresp = (uint32_t) esp;
	regs->eip = (uint32_t) act->sa_handler;

	/* restore sigmask */
	if (!sigisemptyset(&current_task->saved_sigmask)) {
		current_task->blocked = current_task->saved_sigmask;
		sigemptyset(&current_task->saved_sigmask);
	}

	/* signal in system call : return to user mode */
	if (current_task->in_syscall) {
		current_task->in_syscall = 0;
		regs->eax = -EINTR;
		return_user_mode(regs);
	}
}

/*
 * Get next signal.
 */
static int next_signal(struct task *task, sigset_t *mask)
{
	unsigned long i, x, *s, *m;
	int sig = 0;

	/* find first signal */
	s = task->pending.signal.sig;
	m = mask->sig;
	for (i = 0; i < _NSIG_WORDS; i++, s++, m++) {
		x = *s & ~*m;
		if (x != 0) {
			sig = ffz(~x) + i * _NSIG_BPW + 1;
			break;
		}
	}

	return sig;
}

/*
 * Collect a signal.
 */
static int collect_signal(int sig, struct sigpending *list, siginfo_t *info)
{
	struct sigqueue *q, *first = NULL;
	struct list_head *pos;

	/* check pending list */
	if (!sigismember(&list->signal, sig))
		return 0;

	/* find signal in list */
	list_for_each(pos, &list->list) {
		q = list_entry(pos, struct sigqueue, list);
		if (q->info.si_signo == sig) {
			if (first)
				goto still_pending;
			first = q;
		}
	}

	/* delete signal from pending list */
	sigdelset(&list->signal, sig);

	/* collect signal */
	if (first) {
still_pending:
		list_del(&first->list);
		memcpy(info, &q->info, sizeof(siginfo_t));
		kfree(first);
		nr_queued_signals--;
		return 1;
	}

	/* signal not in the list */
	info->si_signo = sig;
	info->si_errno = 0;
	info->si_code = 0;
	info->__si_fields.__si_common.__first.__piduid.si_pid = 0;
	info->__si_fields.__si_common.__first.__piduid.si_uid = 0;
	return 1;
}

/*
 * Dequeue a signal.
 */
static int dequeue_signal(sigset_t *mask, siginfo_t *info)
{
	int sig;

	/* get next signal */
	sig = next_signal(current_task, mask);
	if (!sig)
		return 0;

	/* collect signal */
	if (!collect_signal(sig, &current_task->pending, info))
		sig = 0;

	return sig;
}

/*
 * Notify parent process.
 */
void notify_parent(struct task *task, int sig)
{
	siginfo_t info;

	/* set signal information */
	info.si_signo = sig;
	info.si_errno = 0;
	info.si_code = SI_KERNEL;
	info.__si_fields.__si_common.__first.__piduid.si_pid = task->pid;
	  
	/* send signal */
	send_sig_info(current_task->parent, sig, &info);
	wake_up(&current_task->parent->wait_child_exit);
}

/*
 * Handle a traced signal.
 */
static int ptrace_signal(int sig, siginfo_t *info)
{
	/* process not traced */
	if (!(current_task->ptrace & PT_PTRACED))
		return sig;

	/* let the debugger run */
	current_task->exit_code = sig;
	current_task->state = TASK_STOPPED;
	current_task->last_siginfo = info;
	notify_parent(current_task, SIGCHLD);
	schedule();
	current_task->last_siginfo = NULL;

	/* did the debugger cancel the sig ? */
	sig = current_task->exit_code;
	if (!sig)
		return sig;
	current_task->exit_code = 0;

	/* ingore SIGSTOP */
	if (sig == SIGSTOP)
		return 0;

	/* update signal information */
	if (sig != info->si_signo) {
		info->si_signo = sig;
		info->si_errno = 0;
		info->si_code = SI_USER;
		info->__si_fields.__si_common.__first.__piduid.si_pid = current_task->parent->pid;
		info->__si_fields.__si_common.__first.__piduid.si_uid = current_task->parent->uid;
	}

	/* if the (new) signal is now blocked, requeue it */
	if (sigismember(&current_task->blocked, sig)) {
		send_sig_info(current_task, sig, info);
		return 0;
	}

	return sig;
}

/*
 * Handle signal of current task.
 */
int do_signal(struct registers *regs)
{
	struct sigaction *act;
	siginfo_t info;
	int sig;

	for (;;) {
		/* get first unblocked signal */
		sig = dequeue_signal(&current_task->blocked, &info);
		if (!sig)
			break;

		/* get signal action */
		act = &current_task->sig->action[sig - 1];

		/* traced process */
		if (sig != SIGKILL) {
			sig = ptrace_signal(sig, &info);
			if (!sig)
				continue;
		}

		/* ignore signal handler */
		if (act->sa_handler == SIG_IGN)
			continue;

		/* default signal handler */
		if (act->sa_handler == SIG_DFL) {
			/* init task gets no signals it doesn't want */
			if (current_task->pid == 1)
				continue;

			switch (sig) {
				/* ignore those signals */
				case SIGCONT:
				case SIGCHLD:
				case SIGWINCH:
					continue;
				case SIGSTOP:
				case SIGTSTP:
					current_task->state = TASK_STOPPED;
					current_task->exit_code = sig;
					notify_parent(current_task, SIGCHLD);
					schedule();
					continue;
				default:
					do_exit(sig);
					break;
			}
		}

		handle_signal(regs, sig, act);
		return 1;
	}

	/* restore sigmask */
	if (!sigisemptyset(&current_task->saved_sigmask)) {
		current_task->blocked = current_task->saved_sigmask;
		sigemptyset(&current_task->saved_sigmask);
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

	/* check sigset size */
	if (sigsetsize != sizeof(sigset_t))
		return -EINVAL;

	/* set new sigmask */
	current_task->saved_sigmask = current_task->blocked;
	current_task->blocked = *newset;
	sigdelsetmask(&current_task->blocked, sigmask(SIGKILL) | sigmask(SIGSTOP));

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
	/* check sigset size */
	if (sigsetsize != sizeof(sigset_t))
		return -EINVAL;

	if (!set)
		return -EINVAL;

	/* get pending signals */
	*set = current_task->pending.signal;
	sigandsets(set, &current_task->blocked);

	return 0;
}

/*
 * Sigaction system call (= change action taken by a process on receipt of a specific signal).
 */
int sys_rt_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
	if (signum < 1 || signum > _NSIG)
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
	/* check sigset size */
	if (sigsetsize != sizeof(sigset_t))
		return -EINVAL;

	/* save current sigset */
	if (oldset)
		*oldset = current_task->blocked;

	if (!set)
		return 0;

	/* update sigmask */
	if (how == SIG_BLOCK)
	 	sigorsets(&current_task->blocked, set);
	else if (how == SIG_UNBLOCK)
	 	signandsets(&current_task->blocked, set);
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
	current_task->sig->in_sig = 0;

	/* return value of syscall interrupted by signal */
	return current_task->signal_regs.eax;
}

/*
 * Kill system call (send a signal to a process).
 */
int sys_kill(pid_t pid, int sig)
{
	siginfo_t info;

	/* set signal information */
	memset(&info, 0, sizeof(siginfo_t));
	info.si_signo = sig;
	info.si_errno = 0;
	info.si_code = SI_USER;
	info.__si_fields.__si_common.__first.__piduid.si_pid = current_task->pid;
	info.__si_fields.__si_common.__first.__piduid.si_uid = current_task->uid;

	/* send signal */
	return kill_something_info(pid, sig, &info);
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
int sys_rt_sigtimedwait(const sigset_t *usigset, void *uinfo, const struct old_timespec *uts, size_t sigsetsize)
{
	time_t timeout = MAX_SCHEDULE_TIMEOUT;
	sigset_t these, old_blocked;
	siginfo_t info;
	int sig;

	/* check sigset size */
	if (sigsetsize != sizeof(sigset_t))
		return -EINVAL;

	/* get signal set */
	these = *usigset;
	sigdelsetmask(&these, sigmask(SIGKILL) | sigmask(SIGSTOP));
	signotset(&these);

	/* find signal */
	sig = dequeue_signal(&these, &info);
	if (!sig) {
		old_blocked = current_task->blocked;
		sigandsets(&current_task->blocked, &these);

		/* prepare timeout */
		if (uts)
			timeout = old_timespec_to_jiffies(uts) + (uts->tv_sec || uts->tv_nsec);

		/* goto sleep */
		current_task->state = TASK_SLEEPING;
		timeout = schedule_timeout(timeout);

		/* check signals */
		sig = dequeue_signal(&these, &info);
		current_task->blocked = old_blocked;
	}

	/* no signal */
	if (!sig)
		return timeout ? -EINTR : -EAGAIN;

	/* copy signal informations */
	if (uinfo)
		memcpy(uinfo, &info, sizeof(siginfo_t));

	return sig;
}
