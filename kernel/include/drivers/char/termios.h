#ifndef _TERMIOS_H_
#define _TERMIOS_H_

#include <stddef.h>

#define TCGETS			0x5401
#define TCSETS			0x5402
#define TCSETSW			0x5403
#define TCSETSF			0x5404
#define TCGETA			0x5405
#define TCSETA			0x5406
#define TCSETAW			0x5407
#define TCSETAF			0x5408
#define TCSBRK			0x5409
#define TCXONC			0x540A
#define TCFLSH			0x540B
#define TIOCEXCL		0x540C
#define TIOCNXCL		0x540D
#define TIOCSCTTY		0x540E
#define TIOCGPGRP		0x540F
#define TIOCSPGRP		0x5410
#define TIOCOUTQ		0x5411
#define TIOCSTI			0x5412
#define TIOCGWINSZ		0x5413
#define TIOCSWINSZ		0x5414
#define TIOCMGET		0x5415
#define TIOCMBIS		0x5416
#define TIOCMBIC		0x5417
#define TIOCMSET		0x5418
#define TIOCGSOFTCAR		0x5419
#define TIOCSSOFTCAR		0x541A
#define FIONREAD		0x541B
#define TIOCLINUX		0x541C
#define TIOCINQ			FIONREAD
#define TIOCNOTTY		0x5422
#define TIOCGSID		0x5429
#define TIOCGLCKTRMIOS		0x5456
#define TIOCSLCKTRMIOS		0x5457

#define TIOCSPTLCK		0x40045431
#define TIOCGPTN		0x80045430

#define TCIFLUSH		0
#define TCOFLUSH		1
#define TCIOFLUSH		2

/*
 * Window size structure.
 */
struct winsize {
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel;
	unsigned short ws_ypixel;
};

#define NCCS			17

/*
 * Termios structure.
 */
struct termios {
	tcflag_t	c_iflag;
	tcflag_t	c_oflag;
	tcflag_t	c_cflag;
	tcflag_t	c_lflag;
	cc_t		c_line;
	cc_t		c_cc[NCCS];
};

/* c_cc characters */
#define VINTR			0
#define VQUIT			1
#define VERASE			2
#define VKILL			3
#define VEOF			4
#define VTIME			5
#define VMIN			6
#define VSWTC			7
#define VSTART			8
#define VSTOP			9
#define VSUSP			10
#define VEOL			11
#define VREPRINT		12
#define VDISCARD		13
#define VWERASE		 	14
#define VLNEXT			15
#define VEOL2			16

/* c_iflag bits */
#define IGNBRK			0000001
#define BRKINT			0000002
#define IGNPAR			0000004
#define PARMRK			0000010
#define INPCK			0000020
#define ISTRIP			0000040
#define INLCR			0000100
#define IGNCR			0000200
#define ICRNL			0000400
#define IUCLC			0001000
#define IXON			0002000
#define IXANY			0004000
#define IXOFF			0010000
#define IMAXBEL		 	0020000

/* c_oflag bits */
#define OPOST			0000001
#define OLCUC			0000002
#define ONLCR			0000004
#define OCRNL			0000010
#define ONOCR			0000020
#define ONLRET			0000040
#define OFILL			0000100
#define OFDEL			0000200
#define NLDLY			0000400
#define NL0			0000000
#define NL1			0000400
#define CRDLY			0003000
#define CR0			0000000
#define CR1			0001000
#define CR2			0002000
#define CR3			0003000
#define TABDLY			0014000
#define TAB0			0000000
#define TAB1			0004000
#define TAB2			0010000
#define TAB3			0014000
#define XTABS			0014000
#define BSDLY			0020000
#define BS0			0000000
#define BS1			0020000
#define VTDLY			0040000
#define VT0			0000000
#define VT1			0040000
#define FFDLY			0040000
#define FF0			0000000
#define FF1			0040000

/* c_cflag bit meaning */
#define CBAUD			0000017
#define B0			0000000
#define B50			0000001
#define B75			0000002
#define B110			0000003
#define B134			0000004
#define B150			0000005
#define B200			0000006
#define B300			0000007
#define B600			0000010
#define B1200			0000011
#define B1800			0000012
#define B2400			0000013
#define B4800			0000014
#define B9600			0000015
#define B19200			0000016
#define B38400			0000017
#define EXTA			B19200
#define EXTB			B38400
#define CSIZE			0000060
#define CS5			0000000
#define CS6			0000020
#define CS7			0000040
#define CS8			0000060
#define CSTOPB			0000100
#define CREAD			0000200
#define PARENB			0000400
#define PARODD			0001000
#define HUPCL			0002000
#define CLOCAL			0004000
#define CIBAUD			03600000
#define CRTSCTS		 	020000000000

/* c_lflag bits */
#define ISIG			0000001
#define ICANON			0000002
#define XCASE			0000004
#define ECHO			0000010
#define ECHOE			0000020
#define ECHOK			0000040
#define ECHONL			0000100
#define NOFLSH			0000200
#define TOSTOP			0000400
#define ECHOCTL		 	0001000
#define ECHOPRT		 	0002000
#define ECHOKE			0004000
#define FLUSHO			0010000
#define PENDIN			0040000
#define IEXTEN			0100000

/*
 * intr=^C		quit=^|		 erase=del	 kill=^U
 * eof=^D		 vtime=\0		vmin=\1		 sxtc=\0
 * start=^Q	 stop=^S		 susp=^Z		 eol=\0
 * reprint=^R discard=^U	werase=^W	 lnext=^V
 * eol2=\0
 */
#define INIT_C_CC "\003\034\177\025\004\0\1\0\021\023\032\0\022\017\027\026\0"

#endif
