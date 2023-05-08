#include <fcntl.h>
#include <unistd.h>
#include <stdarg.h>

#include "../x86/__syscall.h"

int fcntl(int fd, int cmd, ...)
{
	unsigned long arg;
	va_list ap;

	va_start(ap, cmd);
	arg = va_arg(ap, unsigned long);
	va_end(ap);

	return __syscall_ret(__syscall3(SYS_fcntl, fd, cmd, arg));
}