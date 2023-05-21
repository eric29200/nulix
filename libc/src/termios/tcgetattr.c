#include <termios.h>
#include <sys/ioctl.h>

int tcgetattr(int fd, struct termios *termios_p)
{
	if (ioctl(fd, TCGETS, termios_p))
		return -1;
	
	return 0;
}