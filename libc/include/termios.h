#ifndef _LIBC_TERMIOS_H_
#define _LIBC_TERMIOS_H_

typedef unsigned char cc_t;
typedef unsigned int speed_t;
typedef unsigned int tcflag_t;

#define NCCS		32

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