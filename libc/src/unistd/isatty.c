#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>

#include "../x86/__syscall.h"

int isatty(int fd)
{
	struct winsize wsz;
	long ret;

	ret = ioctl(fd, TIOCGWINSZ, &wsz);
	if (ret == 0)
		return 1;

	if (errno != EBADF)
		errno = ENOTTY;

	return 0;
}