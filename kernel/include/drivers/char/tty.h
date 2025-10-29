#ifndef _TTY_H_
#define _TTY_H_

#include <drivers/char/console.h>
#include <drivers/char/pty.h>
#include <drivers/char/termios.h>
#include <drivers/video/fb.h>
#include <proc/sched.h>
#include <stderr.h>

#define NR_TTYS			(NR_CONSOLES + NR_PTYS * 2)

#define TTY_BUF_SIZE		1024
#define TTY_ESC_BUF_SIZE	16
#define TTY_DELAY_UPDATE_MS	20

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
#define O_NOCR(tty)		_O_FLAG((tty),ONOCR)
#define O_CRNL(tty)		_O_FLAG((tty),OCRNL)
#define O_NLRET(tty)		_O_FLAG((tty),ONLRET)
#define O_LCUC(tty)		_O_FLAG((tty),OLCUC)

/*
 * Tty queue structure.
 */
struct tty_queue {
	size_t			head;
	size_t			tail;
	uint8_t			buf[TTY_BUF_SIZE];
};

/*
 * TTY driver.
 */
struct tty_driver {
	ssize_t			(*write)(struct tty *);					/* write function */
	int			(*ioctl)(struct tty *, int, unsigned long);		/* ioctl function */
	int			(*close)(struct tty *);					/* close function */
	struct termios		termios;						/* terminal i/o */
};

/*
 * TTY structure.
 */
struct tty {
	dev_t			device;							/* device */
	int			count;							/* reference count */
	pid_t			session;						/* session id */
	pid_t			pgrp;							/* process group id */
	struct tty_queue	read_queue;						/* read queue */
	struct tty_queue	write_queue;						/* write queue */
	struct tty_queue	cooked_queue;						/* cooked queue */
	int			canon_data;						/* canon data */
	size_t			canon_head;						/* canon head */
	struct winsize		winsize;						/* window size */
	struct termios		termios;						/* terminal i/o */
	struct wait_queue *	wait;							/* wait queue */
	struct tty *		link;							/* linked tty */
	struct tty_driver *	driver;							/* tty driver */
	void *			driver_data;						/* tty driver data */
};

/* tty queue functions */
#define tty_queue_left(queue)		(((queue)->tail - (queue)->head - 1) & (TTY_BUF_SIZE - 1))
#define tty_queue_full(queue)		(!tty_queue_left(queue))
#define tty_queue_empty(queue)		((queue)->head == (queue)->tail)

int tty_queue_getc(struct tty_queue *queue);
int tty_queue_putc(struct tty_queue *queue, uint8_t c);

/* tty functions */
int init_tty(struct multiboot_tag_framebuffer *tag_fb);
void tty_init_dev(struct tty *tty, dev_t device, struct tty_driver *driver);
void tty_do_cook(struct tty *tty);
void tty_change(int n);
void tty_complete_change(int n);
void disassociate_ctty();

/* pty controller functions */
int ptmx_open(struct file *filp);

/* global ttys */
extern struct tty tty_table[NR_TTYS];
extern int fg_console;

#endif
