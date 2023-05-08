#include <unistd.h>
#include <errno.h>

#include "../x86/__syscall.h"

void *sbrk(intptr_t increment)
{
	if (increment)
		return (void *) __syscall_ret(-ENOMEM);

	return (void *) __syscall1(SYS_brk, 0);
}
