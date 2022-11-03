#include <drivers/serial.h>
#include <drivers/keyboard.h>
#include <drivers/termios.h>
#include <drivers/tty.h>
#include <drivers/pit.h>
#include <proc/sched.h>
#include <ipc/signal.h>
#include <stdio.h>
#include <stderr.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>
#include <dev.h>
#include <kd.h>

/* global ttys */
struct tty_t tty_table[NR_TTYS];
struct tty_t pty_table[NR_PTYS];
int current_tty;

/*
 * Lookup for a tty.
 */
struct tty_t *tty_lookup(dev_t dev)
{
	int i;

	/* current task tty */
	if (dev == DEV_TTY) {
		if (current_task->tty == DEV_TTY0)
			return current_tty >= 0 ? &tty_table[current_tty] : NULL;

		/* find current task tty or pty */
		switch (major(current_task->tty)) {
			case major(DEV_TTY0):
				for (i = 0; i < NR_TTYS; i++)
					if (current_task->tty == tty_table[i].dev)
						return &tty_table[i];
				break;
			case DEV_PTS_MAJOR:
				for (i = 0; i < NR_PTYS; i++)
					if (current_task->tty == pty_table[i].dev)
						return &pty_table[i];
				break;
			default:
				break;
		}

		return NULL;
	}

	/* current active tty */
	if (dev == DEV_TTY0)
		return current_tty >= 0 ? &tty_table[current_tty] : NULL;

	/* tty */
	if (major(dev) == major(DEV_TTY0) && minor(dev) > 0 && minor(dev) <= NR_TTYS)
		return &tty_table[minor(dev) - 1];

	/* pty */
	if (major(dev) == DEV_PTS_MAJOR && minor(dev) < NR_PTYS)
		return &pty_table[minor(dev)];

	return NULL;
}

/*
 * Open a TTY.
 */
static int tty_open(struct file_t *filp)
{
	struct tty_t *tty;

	/* get tty */
	tty = tty_lookup(filp->f_inode->i_rdev);
	if (!tty)
		return -EINVAL;

	/* attach tty to file */
	filp->f_private = tty;

	/* set current task tty */
	switch (major(filp->f_inode->i_rdev)) {
		case major(DEV_TTY0):
		case DEV_PTS_MAJOR:
			/* set current's task tty */
			current_task->tty = filp->f_inode->i_rdev;

			/* set tty process group */
			tty->pgrp = current_task->pgid;
			break;
		default:
			break;
	}

	return 0;
}

/*
 * Read TTY.
 */
static int tty_read(struct file_t *filp, char *buf, int n)
{
	struct tty_t *tty;
	int count = 0;
	uint8_t c;

	/* get tty */
	tty = filp->f_private;
	if (!tty)
		return -EINVAL;

	/* read all characters */
	while (count < n) {
		/* non blocking mode : returns if no characters in cooked queue */
		if ((filp->f_flags & O_NONBLOCK) && ring_buffer_empty(&tty->cooked_queue))
			return -EAGAIN;

		/* read char */
		ring_buffer_read(&tty->cooked_queue, &c, 1);

		/* add char to buffer */
		((unsigned char *) buf)[count++] = c;

		/* end of line : return */
		if (L_CANON(tty) && c == '\n') {
			tty->canon_data--;
			break;
		}

		/* no more characters : break */
		if (ring_buffer_empty(&tty->cooked_queue))
			break;
	}

	return count;
}

/*
 * Output/Echo a character.
 */
static void out_char(struct tty_t *tty, uint8_t c)
{
	if (ISCNTRL(c) && !ISSPACE(c) && L_ECHOCTL(tty)) {
		ring_buffer_putc(&tty->write_queue, '^');
		ring_buffer_putc(&tty->write_queue, c + 64);
		tty->driver->write(tty);
	} else {
		ring_buffer_putc(&tty->write_queue, c);
		tty->driver->write(tty);
	}
}

/*
 * Cook input characters.
 */
void tty_do_cook(struct tty_t *tty)
{
	uint8_t c;

	while (tty->read_queue.size > 0) {
		/* get next input character */
		ring_buffer_read(&tty->read_queue, &c, 1);

		/* convert to ascii */
		if (I_ISTRIP(tty))
			c = TOASCII(c);

		/* lower case */
		if (I_IUCLC(tty) && ISUPPER(c))
			c = TOLOWER(c);

		/* handle carriage return and new line */
		if (c == '\r') {
			/* ignore carriage return */
			if (I_IGNCR(tty))
				continue;

			/* carriage return = new line */
			if (I_ICRNL(tty))
				c = '\n';
		} else if (c == '\n' && I_INLCR(tty)) {
			c = '\r';
		}

		/* handle signals */
		if (L_ISIG(tty)) {
			if (c == tty->termios.c_cc[VINTR]) {
				task_signal_group(tty->pgrp, SIGINT);
				continue;
			}

			if (c == tty->termios.c_cc[VQUIT]) {
				task_signal_group(tty->pgrp, SIGQUIT);
				continue;
			}

			if (c == tty->termios.c_cc[VSUSP]) {
				task_signal_group(tty->pgrp, SIGSTOP);
				continue;
			}
		}

		/* echo = put character on write queue */
		if (L_ECHO(tty) && !ring_buffer_full(&tty->write_queue))
			out_char(tty, c);

		/* put character in cooked queue */
		if (!ring_buffer_full(&tty->cooked_queue))
			ring_buffer_write(&tty->cooked_queue, &c, 1);

		/* update canon data */
		if (L_CANON(tty) && c == '\n')
			tty->canon_data++;
	}

	/* wake up eventual process */
	task_wakeup(&tty->wait);
}

/*
 * Write to TTY.
 */
static int tty_write(struct file_t *filp, const char *buf, int n)
{
	struct tty_t *tty;
	int i;

	/* get tty */
	tty = filp->f_private;
	if (!tty)
		return -EINVAL;

	/* write not implemented */
	if (!tty->driver->write)
		return -EINVAL;

	/* put characters in write queue */
	for (i = 0; i < n; i++) {
		/* put next character */
		ring_buffer_write(&tty->write_queue, &((uint8_t *) buf)[i], 1);

		/* write to tty */
		if (ring_buffer_full(&tty->write_queue) || i == n - 1)
			tty->driver->write(tty);
	}

	return n;
}

/*
 * Change current tty.
 */
void tty_complete_change(int n)
{
	struct framebuffer_t *fb;
	struct tty_t *tty_new;

	/* check tty */
	if (n < 0 || n >= NR_TTYS || n == current_tty)
		return;

	/* get current tty */
	tty_new = &tty_table[n];

	/* if new console is in process mode, acquire it */
	if (tty_new->vt_mode.mode == VT_PROCESS) {
		/* send acquire signal */
		if (task_signal(tty_new->vt_pid, tty_new->vt_mode.acqsig) != 0)
			reset_vc(tty_new);
	}

	/* disable current frame buffer */
	tty_table[current_tty].fb.active = 0;

	/* refresh new frame buffer */
	if (tty_new->mode == KD_TEXT) {
		fb = &tty_new->fb;
		fb->active = 1;
		fb->ops->update_region(fb, 0, fb->width * fb->height);
	}

	/* set current tty */
	current_tty = n;

	/* wake up eventual processes */
	task_wakeup(&vt_activate_wq);
}

/*
 * Change current tty.
 */
void tty_change(int n)
{
	struct tty_t *tty;

	/* check tty */
	if (n < 0 || n >= NR_TTYS || n == current_tty)
		return;

	/* get current tty */
	tty = &tty_table[current_tty];

	/* in process mode, handshake realase/acquire */
	if (tty->vt_mode.mode == VT_PROCESS) {
		/* send release signal */
		if (task_signal(tty->vt_pid, tty->vt_mode.relsig) == 0) {
			tty->vt_newvt = n;
			return;
		}

		/* on error reset console */
		reset_vc(tty);
	}

	/* ignore switches in KD_GRAPHICS mode */
	if (tty->mode == KD_GRAPHICS)
		return;

	/* change tty */
	tty_complete_change(n);
}

/*
 * TTY ioctl.
 */
int tty_ioctl(struct file_t *filp, int request, unsigned long arg)
{
	struct tty_t *tty;
	int ret;

	/* get tty */
	tty = filp->f_private;
	if (!tty)
		return -EINVAL;

	switch (request) {
		case TCGETS:
			memcpy((struct termios_t *) arg, &tty->termios, sizeof(struct termios_t));
			break;
		case TCSETS:
		case TCSETSW:
		case TCSETSF:
			memcpy(&tty->termios, (struct termios_t *) arg, sizeof(struct termios_t));
			tty->canon_data = 0;
			break;
		case TIOCGWINSZ:
			memcpy((struct winsize_t *) arg, &tty->winsize, sizeof(struct winsize_t));
			break;
		case TIOCSWINSZ:
			memcpy(&tty->winsize, (struct winsize_t *) arg, sizeof(struct winsize_t));
			break;
		case TIOCGPGRP:
			*((pid_t *) arg) = tty->pgrp;
			break;
		case TIOCSPGRP:
			tty->pgrp = *((pid_t *) arg);
			break;
		default:
			if (tty->driver->ioctl) {
				ret = tty->driver->ioctl(tty, request, arg);
				if (ret != -ENOIOCTLCMD)
					return ret;
			}

			printf("Unknown ioctl request (%x) on device %x\n", request, (int) filp->f_inode->i_rdev);
			break;
	}

	return 0;
}

/*
 * Check if there is some data to read.
 */
static int tty_input_available(struct tty_t *tty)
{
	if (L_CANON(tty))
		return tty->canon_data > 0;

	return tty->cooked_queue.size > 0;
}

/*
 * Poll a tty.
 */
static int tty_poll(struct file_t *filp, struct select_table_t *wait)
{
	struct tty_t *tty;
	int mask = 0;

	/* get tty */
	tty = filp->f_private;
	if (!tty)
		return -EINVAL;

	/* check if there is some characters to read */
	if (tty_input_available(tty))
		mask |= POLLIN;

	/* check if there is some characters to write */
	if (!ring_buffer_full(&tty->write_queue))
		mask |= POLLOUT;

	/* add wait queue to select table */
	select_wait(&tty->wait, wait);

	return mask;
}

/*
 * Init tty attributes.
 */
static void tty_init_attr(struct tty_t *tty)
{
	tty->def_color = TEXT_COLOR(TEXT_BLACK, TEXT_LIGHT_GREY);
	tty->color = tty->def_color;
	tty->intensity = 1;
	tty->reverse = 0;
	tty->erase_char = ' ' | (tty->def_color << 8);
	tty->deccm = 1;
	tty->attr = tty->color;
}

/*
 * Default tty attributes.
 */
void tty_default_attr(struct tty_t *tty)
{
	tty->intensity = 1;
	tty->reverse = 0;
	tty->color = tty->def_color;
}

/*
 * Update tty attributes.
 */
void tty_update_attr(struct tty_t *tty)
{
	tty->attr = tty->color;

	if (tty->reverse)
		tty->attr = TEXT_COLOR(TEXT_COLOR_FG(tty->color), TEXT_COLOR_BG(tty->color));
	if (tty->intensity == 2)
		tty->attr ^= 0x08;

	/* redefine erase char */
	tty->erase_char = ' ' | (tty->color << 8);
}

/*
 * Init a tty.
 */
int tty_init_dev(struct tty_t *tty, dev_t dev, struct tty_driver_t *driver, struct multiboot_tag_framebuffer *tag_fb)
{
	int ret;

	tty->dev = dev;
	tty->pgrp = 0;
	tty->wait = NULL;
	tty->link = NULL;
	tty->driver = driver;
	reset_vc(tty);
	tty_init_attr(tty);

	/* init read queue */
	ret = ring_buffer_init(&tty->read_queue, TTY_BUF_SIZE);
	if (ret)
		return ret;

	/* init write queue */
	ret = ring_buffer_init(&tty->write_queue, TTY_BUF_SIZE);
	if (ret)
		return ret;

	/* init cooked queue */
	ret = ring_buffer_init(&tty->cooked_queue, TTY_BUF_SIZE);
	if (ret)
		return ret;

	/* init frame buffer */
	if (tag_fb) {
		ret = init_framebuffer(&tty->fb, tag_fb, tty->erase_char, 0);
		if (ret)
			return ret;

		/* set winsize */
		tty->winsize.ws_row = tty->fb.height - 1;
		tty->winsize.ws_col = tty->fb.width - 1;
	}

	/* init termios */
	tty->termios = (struct termios_t) {
		.c_iflag	= ICRNL,
		.c_oflag	= OPOST | ONLCR,
		.c_cflag	= 0,
		.c_lflag	= IXON | ISIG | ICANON | ECHO | ECHOCTL | ECHOKE,
		.c_line		= 0,
		.c_cc		= INIT_C_CC,
	};

	return 0;
}

/*
 * Destroy a tty.
 */
static void tty_destroy(struct tty_t *tty)
{
	ring_buffer_destroy(&tty->read_queue);
	ring_buffer_destroy(&tty->write_queue);
	ring_buffer_destroy(&tty->cooked_queue);
}

/*
 * Init TTYs.
 */
int init_tty(struct multiboot_tag_framebuffer *tag_fb)
{
	int i, ret;

	/* reset ttys */
	for (i = 0; i < NR_TTYS; i++)
		memset(&tty_table[i], 0, sizeof(struct tty_t));

	/* init consoles */
	for (i = 0; i < NR_TTYS; i++) {
		ret = tty_init_dev(&tty_table[i], DEV_TTY0 + i + 1, &console_driver, tag_fb);
		if (ret)
			goto err;
	}

	/* set current tty to first tty */
	current_tty = 0;
	tty_table[current_tty].fb.active = 1;

	/* init ptys */
	init_pty();

	return 0;
err:
	/* destroy ttys */
	for (i = 0; i < NR_TTYS; i++)
		tty_destroy(&tty_table[i]);
	return ret;
}

/*
 * Tty file operations.
 */
static struct file_operations_t tty_fops = {
	.open		= tty_open,
	.read		= tty_read,
	.write		= tty_write,
	.poll		= tty_poll,
	.ioctl		= tty_ioctl,
};

/*
 * Tty inode operations.
 */
struct inode_operations_t tty_iops = {
	.fops		= &tty_fops,
};
