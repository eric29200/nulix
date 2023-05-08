#include <sys/ioctl.h>
#include <stdarg.h>
#include <unistd.h>

#include "../x86/__syscall.h"

int ioctl(int fd, unsigned long request, ...)
{
	unsigned long arg;
	va_list ap;

	va_start(ap, request);
	arg = va_arg(ap, unsigned long);
	va_end(ap);

	return __syscall_ret(__syscall3(SYS_ioctl, fd, request, arg));
}