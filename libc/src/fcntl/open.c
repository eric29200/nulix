#include <unistd.h>
#include <stdarg.h>
#include <fcntl.h>

#include "../x86/__syscall.h"

int open(const char *pathname, int flags, ...)
{
	mode_t mode = 0;
	va_list ap;
	int fd;

	if ((flags & O_CREAT) || (flags & O_TMPFILE) == O_TMPFILE) {
		va_start(ap, flags);
		mode = va_arg(ap, mode_t);
		va_end(ap);
	}

	fd = __syscall3(SYS_open, (long) pathname, flags, mode);
	if (fd >= 0 && (flags & O_CLOEXEC))
		fcntl(fd, F_SETFD, FD_CLOEXEC);

	return fd;
}