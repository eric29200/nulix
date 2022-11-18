#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include <drivers/fb.h>

#define NR_CONSOLES		4

#define TTY_STATE_NORMAL	0
#define TTY_STATE_ESCAPE	1
#define TTY_STATE_SQUARE	2
#define TTY_STATE_GETPARS	3
#define TTY_STATE_GOTPARS	4
#define TTY_STATE_SET_G0	5
#define TTY_STATE_SET_G1	6

#define NPARS			16

/* translation maps */
#define LAT1_MAP		0
#define GRAF_MAP		1
#define IBMPC_MAP		2
#define USER_MAP		3

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

/*
 * Console structure.
 */
struct vc_t {
	int			vc_num;							/* console id */
	uint32_t		vc_pars[NPARS];						/* escaped pars */
	uint32_t		vc_npars;						/* number of escaped pars */
	int			vc_ques;						/* question ? */
	int			vc_state;						/* tty state (NORMAL or ESCAPE) */
	uint8_t			vc_attr;						/* attributes = current color */
	uint8_t			vc_color;						/* forgeground/background color */
	uint8_t			vc_def_color;						/* default foreground/background color */
	uint8_t			vc_intensity;						/* foreground intensity */
	uint8_t			vc_underline;						/* underline */
	uint8_t			vc_reverse;						/* reverse mode */
	uint16_t		vc_erase_char;						/* erase character */
	uint8_t			vc_deccm:1;						/* cursor visible */
	uint8_t			vc_charset;						/* charset (g0 or g1) */
	uint16_t *		vc_translate;						/* translation table */
	uint8_t			vc_charset_g0;						/* g0 charset */
	uint8_t			vc_charset_g1;						/* g1 charset */
	uint8_t			vc_mode;						/* console mode (KD_TEXT or KD_GRAPHICS) */
	char			vc_need_wrap;						/* 1 if console needs to be wrapped */
	struct vt_mode		vt_mode;						/* vt mode (AUTO or PROCESS) */
	pid_t			vt_pid;							/* vt pid */
	int			vt_newvt;						/* new asked vt */
	struct framebuffer_t	fb;							/* framebuffer */
};

extern uint16_t console_translations[][256];
extern struct wait_queue_t *vt_activate_wq;

int init_console();
void console_change(int n);

#endif
