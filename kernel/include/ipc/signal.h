#ifndef _SIGNAL_H_
#define _SIGNAL_H_

#include <stddef.h>
#include <string.h>
#include <time.h>
#include <x86/system.h>

#define _NSIG		64
#define _NSIG_BPW	32
#define _NSIG_WORDS	(_NSIG / _NSIG_BPW)

#define NSIG		32

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
#define SIGUNUSED	31

#define SIG_BLOCK	 0
#define SIG_UNBLOCK	 1
#define SIG_SETMASK	 2

#define SIG_DFL		((sighandler_t) 0)	/* default signal handler */
#define SIG_IGN		((sighandler_t) 1)	/* ignore signal handler */
#define SIG_ERR		((sighandler_t) -1)	/* error signal handler */

#define sigmask(sig)	(1UL << ((sig) - 1))

typedef void (*sighandler_t)(int);

/*
 * Old signal set.
 */
typedef unsigned long old_sigset_t;

/*
 * Signal set.
 */
typedef struct {
	unsigned long	sig[_NSIG_WORDS];
} sigset_t;

/*
 * Signal value.
 */
union sigval {
	int	sival_int;
	void *	sival_ptr;
};

/*
 * Signal information.
 */
typedef struct {
	int si_signo, si_errno, si_code;
	union {
		char __pad[128 - 2 * sizeof(int) - sizeof(long)];
		struct {
			union {
				struct {
					pid_t si_pid;
					uid_t si_uid;
				} __piduid;
				struct {
					int si_timerid;
					int si_overrun;
				} __timer;
			} __first;
			union {
				union sigval si_value;
				struct {
					int si_status;
					time_t si_utime, si_stime;
				} __sigchld;
			} __second;
		} __si_common;
		struct {
			void *si_addr;
			short si_addr_lsb;
			union {
				struct {
					void *si_lower;
					void *si_upper;
				} __addr_bnd;
				unsigned si_pkey;
			} __first;
		} __sigfault;
		struct {
			long si_band;
			int si_fd;
		} __sigpoll;
		struct {
			void *si_call_addr;
			int si_syscall;
			unsigned si_arch;
		} __sigsys;
	} __si_fields;
} siginfo_t;

/*
 * Signal action structure.
 */
struct sigaction {
	sighandler_t	sa_handler;
	sigset_t	sa_mask;
	int		sa_flags;
	void		(*sa_restorer)(void);
};

/*
 * Check if a signal set is empty.
 */
static inline int sigisemptyset(sigset_t *set)
{
	switch (_NSIG_WORDS) {
		case 1:
			return set->sig[0] == 0;
		case 2:
			return (set->sig[1] | set->sig[0]) == 0;
		case 4:
			return (set->sig[3] | set->sig[2] | set->sig[1] | set->sig[0]) == 0;
		default:
			return 0;
	}
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
static inline int sigismember(const sigset_t *set, int _sig)
{
	unsigned long sig = _sig - 1;

	if (_NSIG_WORDS == 1)
		return (set->sig[0] >> sig) & 1;
	else
		return (set->sig[sig / _NSIG_BPW] >> (sig % _NSIG_BPW)) & 1;
}

/*
 * Add a signal to a signal set.
 */
static inline void sigaddset(sigset_t *set, int _sig)
{
	unsigned long sig = _sig - 1;

	if (_NSIG_WORDS == 1)
		set->sig[0] |= 1UL << sig;
	else
		set->sig[sig / _NSIG_BPW] |= 1UL << (sig % _NSIG_BPW);
}

/*
 * Delete a signal from a signal set.
 */
static inline void sigdelset(sigset_t *set, int _sig)
{
	unsigned long sig = _sig - 1;

	if (_NSIG_WORDS == 1)
		set->sig[0] &= ~(1UL << sig);
	else
		set->sig[sig / _NSIG_BPW] &= ~(1UL << (sig % _NSIG_BPW));
}

/*
 * Delete signals from a signal set.
 */
static inline void sigdelsetmask(sigset_t *set, unsigned long mask)
{
	set->sig[0] &= ~mask;
}

/*
 * Negate a signal set.
 */
static inline void signotset(sigset_t *set)
{
	switch (_NSIG_WORDS) {
		case 1:
			set->sig[0] = ~set->sig[0];
			break;
		case 2:
			set->sig[0] = ~set->sig[0];
			set->sig[1] = ~set->sig[1];
			break;
		case 4:
			set->sig[0] = ~set->sig[0];
			set->sig[1] = ~set->sig[1];
			set->sig[3] = ~set->sig[3];
			set->sig[2] = ~set->sig[2];
			break;
		default:
			break;
	}
}

/*
 * Or 2 signal sets.
 */
static inline void sigorsets(sigset_t *res, const sigset_t *oth)
{
	switch (_NSIG_WORDS) {
		case 1:
			res->sig[0] |= oth->sig[0];
			break;
		case 2:
			res->sig[0] |= oth->sig[0];
			res->sig[1] |= oth->sig[1];
			break;
		case 4:
			res->sig[0] |= oth->sig[0];
			res->sig[1] |= oth->sig[1];
			res->sig[3] |= oth->sig[3];
			res->sig[2] |= oth->sig[2];
			break;
		default:
			break;
	}
}

/*
 * And 2 signal sets.
 */
static inline void sigandsets(sigset_t *res, const sigset_t *oth)
{
	switch (_NSIG_WORDS) {
		case 1:
			res->sig[0] &= oth->sig[0];
			break;
		case 2:
			res->sig[0] &= oth->sig[0];
			res->sig[1] &= oth->sig[1];
			break;
		case 4:
			res->sig[0] &= oth->sig[0];
			res->sig[1] &= oth->sig[1];
			res->sig[3] &= oth->sig[3];
			res->sig[2] &= oth->sig[2];
			break;
		default:
			break;
	}
}

/*
 * Negative and 2 signal sets.
 */
static inline void signandsets(sigset_t *res, const sigset_t *oth)
{
	switch (_NSIG_WORDS) {
		case 1:
			res->sig[0] &= ~oth->sig[0];
			break;
		case 2:
			res->sig[0] &= ~oth->sig[0];
			res->sig[1] &= ~oth->sig[1];
			break;
		case 4:
			res->sig[0] &= ~oth->sig[0];
			res->sig[1] &= ~oth->sig[1];
			res->sig[3] &= ~oth->sig[3];
			res->sig[2] &= ~oth->sig[2];
			break;
		default:
			break;
	}
}

int send_sig(struct task *task, int sig);

int do_signal(struct registers *regs);
int sys_rt_sigprocmask(int how, const sigset_t *set, sigset_t *oldset, size_t sigsetsize);
int sys_rt_sigsuspend(sigset_t *newset, size_t sigsetsize);
int sys_rt_sigtimedwait(const sigset_t *sigset, void *info, const struct old_timespec *uts, size_t sigsetsize);
int sys_rt_sigpending(sigset_t *set, size_t sigsetsize);
int sys_rt_sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
int sys_sigreturn();
int sys_kill(pid_t pid, int sig);
int sys_tkill(pid_t pid, int sig);

#endif
