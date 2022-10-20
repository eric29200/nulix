#ifndef _TTY_H_
#define _TTY_H_

#include <drivers/fb.h>
#include <drivers/termios.h>
#include <lib/ring_buffer.h>
#include <proc/wait.h>

#define TTY_BUF_SIZE		1024
#define TTY_ESC_BUF_SIZE	16
#define TTY_DELAY_UPDATE_MS	20

#define TTY_STATE_NORMAL	0
#define TTY_STATE_ESCAPE	1
#define TTY_STATE_SQUARE	2
#define TTY_STATE_GETPARS	3
#define TTY_STATE_GOTPARS	4

#define NPARS			16

#define _L_FLAG(tty,f)		((tty)->termios.c_lflag & f)
#define _I_FLAG(tty,f)		((tty)->termios.c_iflag & f)
#define _O_FLAG(tty,f)		((tty)->termios.c_oflag & f)

#define L_CANON(tty)		_L_FLAG((tty),ICANON)
#define L_ISIG(tty)		_L_FLAG((tty),ISIG)
#define L_ECHO(tty)		_L_FLAG((tty),ECHO)
#define L_ECHOE(tty)		_L_FLAG((tty),ECHOE)
#define L_ECHOK(tty)		_L_FLAG((tty),ECHOK)
#define L_ECHOCTL(tty)		_L_FLAG((tty),ECHOCTL)
#define L_ECHOKE(tty)		_L_FLAG((tty),ECHOKE)
#define L_TOSTOP(tty)		_L_FLAG((tty),TOSTOP)

#define I_IGNBRK(tty)		_I_FLAG((tty),IGNBRK)
#define I_BRKINT(tty)		_I_FLAG((tty),BRKINT)
#define I_IGNPAR(tty)		_I_FLAG((tty),IGNPAR)
#define I_PARMRK(tty)		_I_FLAG((tty),PARMRK)
#define I_INPCK(tty)		_I_FLAG((tty),INPCK)
#define I_ISTRIP(tty)		_I_FLAG((tty),ISTRIP)
#define I_INLCR(tty)		_I_FLAG((tty),INLCR)
#define I_IGNCR(tty)		_I_FLAG((tty),IGNCR)
#define I_ICRNL(tty)		_I_FLAG((tty),ICRNL)
#define I_IUCLC(tty)		_I_FLAG((tty),IUCLC)
#define I_IXON(tty)		_I_FLAG((tty),IXON)
#define I_IXANY(tty)		_I_FLAG((tty),IXANY)
#define I_IXOFF(tty)		_I_FLAG((tty),IXOFF)
#define I_IMAXBEL(tty)		_I_FLAG((tty),IMAXBEL)

#define O_POST(tty)		_O_FLAG((tty),OPOST)
#define O_NLCR(tty)		_O_FLAG((tty),ONLCR)
#define O_CRNL(tty)		_O_FLAG((tty),OCRNL)
#define O_NLRET(tty)		_O_FLAG((tty),ONLRET)
#define O_LCUC(tty)		_O_FLAG((tty),OLCUC)

/*
 * TTY structure.
 */
struct tty_t {
	dev_t			dev;							/* dev number */
	pid_t			pgrp;							/* process group id */
	struct ring_buffer_t	read_queue;						/* read queue */
	struct ring_buffer_t	write_queue;						/* write queue */
	struct ring_buffer_t	cooked_queue;						/* cooked queue */
	uint32_t		pars[NPARS];						/* escaped pars */
	uint32_t		npars;							/* number of escaped pars */
	int			esc_buf_size;						/* escape buffer size */
	int			state;							/* tty state (NORMAL or ESCAPE) */
	uint8_t			attr;							/* attributes = current color */
	uint8_t			color;							/* forgeground/background color */
	uint8_t			def_color;						/* default foreground/background color */
	uint8_t			intensity;						/* foreground intensity */
	uint8_t			underline;						/* underline */
	uint8_t			reverse;						/* reverse mode */
	uint16_t		erase_char;						/* erase character */
	uint8_t			deccm:1;						/* cursor visible */
	int			canon_data;						/* canon data */
	struct winsize_t	winsize;						/* window size */
	struct termios_t	termios;						/* terminal i/o */
	struct framebuffer_t	fb;							/* framebuffer of the tty */
	struct wait_queue_t *	wait;							/* wait queue */
	void			(*write)(struct tty_t *);				/* write function */
	int			(*ioctl)(struct tty_t *, int, unsigned long);		/* ioctl function */
};

int init_tty(struct multiboot_tag_framebuffer *tag_fb);
struct tty_t *tty_lookup(dev_t dev);
void tty_do_cook(struct tty_t *tty);
void tty_change(int n);
void tty_default_attr(struct tty_t *tty);
void tty_update_attr(struct tty_t *tty);

extern struct inode_operations_t tty_iops;

#endif
