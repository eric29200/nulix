#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "../x86/__syscall.h"

int __sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
	struct k_sigaction k_act, k_oldact;
	int ret;

	/* set kernel action */
	if (act) {
		k_act.handler = act->sa_handler;
		k_act.flags = act->sa_flags;
		memcpy(&k_act.mask, &act->sa_mask, NSIG / 8);
	}

	/* do sigaction syscall */
	ret = __syscall4(SYS_rt_sigaction, signum, act ? (long) &k_act : 0, oldact ? (long) &k_oldact : 0, NSIG / 8);

	/* set old action */
	if (oldact && ret == 0) {
		oldact->sa_handler = k_oldact.handler;
		oldact->sa_flags = k_oldact.flags;
		memcpy(&oldact->sa_mask, &k_oldact.mask, NSIG / 8);
	}

	/* return from syscall */
	return __syscall_ret(ret);
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact)
{
	unsigned long set [NSIG / (8 * sizeof(long))];
	int ret;

	/* check signal number */
	if (signum - 32U < 3 || signum - 1U >= NSIG - 1) {
		errno = EINVAL;
		return -1;
	}

	/* SIGABRT : need to block all signals during operation */
	if (signum == SIGABRT)
		__block_all_sigs(&set);

	/* perform sigaction */
	ret = __sigaction(signum, act, oldact);

	/* SIGABRT : restore signals */
	if (signum == SIGABRT)
		__restore_sigs(&set);

	return ret;
}