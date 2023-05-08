#ifndef _LIBC_TERMIOS_H_
#define _LIBC_TERMIOS_H_

/*
 * Window size structure.
 */
struct winsize {
	unsigned short ws_row;
	unsigned short ws_col;
	unsigned short ws_xpixel;
	unsigned short ws_ypixel;
};

#endif