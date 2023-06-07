#include <sys/syscall.h>
#include <stderr.h>

/*
 * Sigaction system call (= change action taken by a process on receipt of a specific signal).
 */
int sys_sigaction(int signum, const struct sigaction_t *act, struct sigaction_t *oldact)
{
	return do_sigaction(signum, act, oldact);
}
