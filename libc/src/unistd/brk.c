#include <unistd.h>
#include <errno.h>

#include "../x86/__syscall.h"

int brk(void *addr)
{
	return __syscall_ret(-ENOMEM);
}