#include <termios.h>
#include <errno.h>
#include <sys/ioctl.h>

int tcsetattr(int fd, int actions, const struct termios *termios_p)
{
	if (actions < 0 || actions > 2) {
		errno = EINVAL;
		return -1;

	}
	
	return ioctl(fd, TCSETS + actions, termios_p);
}