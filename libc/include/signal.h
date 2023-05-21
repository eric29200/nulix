#ifndef _LIBC_SIGNAL_H_
#define _LIBC_SIGNAL_H_

#include <stdio.h>

#define SIGHUP		1
#define SIGINT		2
#define SIGQUIT		3
#define SIGILL		4
#define SIGTRAP	 	5
#define SIGABRT	 	6
#define SIGIOT		SIGABRT
#define SIGBUS		7
#define SIGFPE		8
#define SIGKILL		9
#define SIGUSR1		10
#define SIGSEGV	 	11
#define SIGUSR2	 	12
#define SIGPIPE	 	13
#define SIGALRM	 	14
#define SIGTERM	 	15
#define SIGSTKFLT	16
#define SIGCHLD	 	17
#define SIGCONT	 	18
#define SIGSTOP	 	19
#define SIGTSTP	 	20
#define SIGTTIN	 	21
#define SIGTTOU	 	22
#define SIGURG		23
#define SIGXCPU	 	24
#define SIGXFSZ	 	25
#define SIGVTALRM	26
#define SIGPROF	 	27
#define SIGWINCH	28
#define SIGIO		29
#define SIGPOLL	 	29
#define SIGPWR		30
#define SIGSYS		31
#define SIGUNUSED 	SIGSYS
#define NSIG		64

#define SA_NOCLDSTOP	0x00000001
#define SA_NOCLDWAIT	0x00000002
#define SA_SIGINFO	0x00000004
#define SA_ONSTACK	0x08000000
#define SA_RESTART	0x10000000
#define SA_NODEFER	0x40000000
#define SA_RESETHAND	0x80000000

#define SA_NOMASK	SA_NODEFER
#define SA_ONESHOT	SA_RESETHAND

#define SIG_BLOCK	0
#define SIG_UNBLOCK	1
#define SIG_SETMASK	2

#define SIG_ERR		((sighandler_t) - 1)

typedef void (*sighandler_t)(int);
typedef struct __sigset_t { unsigned long __bits[128 / sizeof(long)]; } sigset_t;

/*
 * Signal value.
 */
union sigval {
	int		sival_int;
	void *		sival_ptr;
};

/*
 * Signal informations.
 */
typedef struct {
	int						si_signo;
	int						si_errno;
	int						si_code;
	union {
		char					__pad[128 * 2 * sizeof(int) - sizeof(long)];
		struct {
			union {
				struct {
					pid_t		si_pid;
					uid_t		si_uid;
				} __piduid;
				struct {
					int		si_timerid;
					int		si_overrun;
				} __timer;
			} __first;
			union {
				union sigval		si_value;
				struct {
					int		si_status;
					clock_t 	si_utime;
					clock_t 	si_stime;
				} __sigchld;
			} __second;
		} __si_common;
		struct {
			void *				si_addr;
			short				si_addr_lsb;
			union {
				struct {
					void *		si_lower;
					void *		si_upper;
				} __addr_bnd;
				unsigned		si_pkey;
			} __first;
		} __sigfault;
		struct {
			long				si_band;
			int				si_fd;
		} __sigpoll;
		struct {
			void *				si_call_addr;
			int				si_syscall;
			unsigned			si_arch;
		} __sigsys;
	} __si_fields;
} siginfo_t;

/*
 * Kernel signal action.
 */
struct k_sigaction {
	void		(*handler)(int);
	unsigned long	flags;
	void		(*restorer)(void);
	unsigned	mask[2];
	void *		unused;
};

/*
 * Signal action.
 */
struct sigaction {
	union {
		void (*sa_handler)(int);
		void (*sa_sigaction)(int, siginfo_t *, void *);
	} __sa_handler;
	sigset_t	sa_mask;
	int		sa_flags;
	void		(*sa_restorer)(void);
};

#define sa_handler	__sa_handler.sa_handler
#define sa_sigaction	__sa_handler.sa_sigaction


void __block_all_sigs(void *set);
void __restore_sigs(void *set);

int kill(pid_t pid, int sig);
sighandler_t signal(int signum, sighandler_t handler);
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);

#endif