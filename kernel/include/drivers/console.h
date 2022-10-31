#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include <drivers/tty.h>

/* ioctls */
#define VT_GETSTATE		0x5603
#define VT_ACTIVATE		0x5606

/*
 * Console status.
 */
struct vt_stat {
	uint16_t	v_active;	/* active console */
	uint16_t	v_signal;	/* signal to send */
	uint16_t	v_state;	/* consoles bitmask */
};

void console_write(struct tty_t *tty);
int console_ioctl(struct tty_t *tty, int request, unsigned long arg);

#endif
