#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include <stddef.h>

/* ioctls */
#define VT_GETMODE		0x5601
#define VT_SETMODE		0x5602
#define VT_GETSTATE		0x5603
#define VT_RELDISP		0x5605
#define VT_ACTIVATE		0x5606
#define VT_WAITACTIVE		0x5607

#define	VT_AUTO			0x00	/* auto vt switching */
#define	VT_PROCESS		0x01	/* process controls switching */
#define	VT_ACKACQ		0x02	/* acknowledge switch */


/*
 * Console status.
 */
struct vt_stat {
	uint16_t	v_active;	/* active console */
	uint16_t	v_signal;	/* signal to send */
	uint16_t	v_state;	/* consoles bitmask */
};

/*
 * Console mode.
 */
struct vt_mode {
	uint8_t		mode;		/* vt mode */
	uint8_t		waitv;		/* if set, hang on writes if not active */
	uint16_t	relsig;		/* signal to raise on release req */
	uint16_t	acqsig;		/* signal to raise on acquisition */
	uint16_t	frsig;		/* unused (set to 0) */
};

extern struct wait_queue_t *vt_activate_wq;

#endif
