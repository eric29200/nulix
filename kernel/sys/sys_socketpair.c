#include <sys/syscall.h>

/*
 * Create a paire of connected sockets.
 */
int sys_socketpair(int domain, int type, int protocol, int sv[2])
{
	return do_socketpair(domain, type, protocol, sv);
}
