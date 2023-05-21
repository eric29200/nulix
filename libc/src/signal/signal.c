#include <signal.h>

sighandler_t signal(int signum, sighandler_t handler)
{
	struct sigaction sa_old, sa = { .sa_handler = handler, .sa_flags = SA_RESTART };

	if (sigaction(signum, &sa, &sa_old) < 0)
		return SIG_ERR;

	return sa_old.sa_handler;
}