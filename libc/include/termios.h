#ifndef _LIBC_TERMIOS_H_
#define _LIBC_TERMIOS_H_

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

#define NCCS		32

#define ISIG		0000001
#define ICANON		0000002
#define ECHO		0000010
#define ECHOE		0000020
#define ECHOK		0000040
#define ECHONL		0000100
#define NOFLSH		0000200
#define TOSTOP		0000400
#define IEXTEN		0100000

#define TCSANOW		0
#define TCSADRAIN	1
#define TCSAFLUSH	2

/*
 * Window size structure.
 */
struct winsize {
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel;
	unsigned short ws_ypixel;
};

/*
 * Term I/O structure.
 */
struct termios {
	tcflag_t	c_iflag;
	tcflag_t	c_oflag;
	tcflag_t	c_cflag;
	tcflag_t	c_lflag;
	cc_t		c_line;
	cc_t		c_cc[NCCS];
	speed_t		__c_ispeed;
	speed_t		__c_ospeed;
};

int tcgetattr(int fd, struct termios *termios_p);
int tcsetattr(int fd, int actions, const struct termios *termios_p);

#endif