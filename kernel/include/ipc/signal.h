#ifndef _SIGNAL_H_
#define _SIGNAL_H_

#include <stddef.h>
#include <string.h>
#include <time.h>
#include <x86/system.h>

#define NSIGS		(SIGUNUSED + 1)

#define SIGHUP		1
#define SIGINT		2
#define SIGQUIT		3
#define SIGILL		4
#define SIGTRAP		5
#define SIGABRT		6
#define SIGIOT		SIGABRT
#define SIGBUS		7
#define SIGFPE		8
#define SIGKILL		9
#define SIGUSR1		10
#define SIGSEGV		11
#define SIGUSR2		12
#define SIGPIPE		13
#define SIGALRM		14
#define SIGTERM		15
#define SIGSTKFLT	16
#define SIGCHLD		17
#define SIGCONT		18
#define SIGSTOP		19
#define SIGTSTP		20
#define SIGTTIN		21
#define SIGTTOU		22
#define SIGURG		23
#define SIGXCPU		24
#define SIGXFSZ		25
#define SIGVTALRM	26
#define SIGPROF		27
#define SIGWINCH	28
#define SIGIO		29
#define SIGPOLL		29
#define SIGPWR		30
#define SIGSYS		31
#define SIGUNUSED	SIGSYS

#define SIG_BLOCK	 0
#define SIG_UNBLOCK	 1
#define SIG_SETMASK	 2

#define SIG_DFL		((sighandler_t) 0)	/* default signal handler */
#define SIG_IGN		((sighandler_t) 1)	/* ignore signal handler */
#define SIG_ERR		((sighandler_t) -1)	/* error signal handler */

#define sigmask(sig)	(1UL << ((sig) - 1))

typedef void (*sighandler_t)(int);

/*
 * Signal action structure.
 */
struct sigaction_t {
	sighandler_t	sa_handler;
	int		sa_flags;
	sigset_t	sa_mask;
};

/*
 * Check if a signal set is empty.
 */
static inline int sigisemptyset(sigset_t *set)
{
	return (int) *set == 0;
}

/*
 * Clear/empty a signal set.
 */
static inline void sigemptyset(sigset_t *set)
{
	memset(set, 0, sizeof(sigset_t));
}

/*
 * Check if sig is a member of sigset.
 */
static inline int sigismember(const sigset_t *set, int sig)
{
	unsigned int s = sig - 1;

	if (s >= NSIGS - 1)
		return 0;

	return 1 & (*set >> sig);
}

/*
 * Add a signal to a signal set.
 */
static inline int sigaddset(sigset_t *set, int sig)
{
	unsigned int s = sig - 1;

	if (s >= NSIGS - 1)
		return -1;

	*set |= (1 << sig);

	return 0;
}

/*
 * Delete a signal from a signal set.
 */
static inline int sigdelset(sigset_t *set, int sig)
{
	unsigned int s = sig - 1;

	if (s >= NSIGS - 1)
		return -1;

	*set &= ~(1 << sig);

	return 0;
}

/*
 * Delete signals from a signal set.
 */
static inline void sigdelsetmask(sigset_t *set, unsigned long mask)
{
	*set &= ~mask;
}

int do_signal(struct registers_t *regs);;
int sys_rt_sigprocmask(int how, const sigset_t *set, sigset_t *oldset, size_t sigsetsize);
int sys_rt_sigsuspend(sigset_t *newset, size_t sigsetsize);
int sys_rt_sigtimedwait(const sigset_t *sigset, void *info, const struct timespec_t *uts, size_t sigsetsize);
int sys_sigaction(int signum, const struct sigaction_t *act, struct sigaction_t *oldact);
int sys_sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int sys_sigreturn();
int sys_kill(pid_t pid, int sig);
int sys_tkill(pid_t pid, int sig);

#endif
