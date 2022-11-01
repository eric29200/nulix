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
#include <time.h>
#include <dev.h>
#include <kd.h>

/* global ttys */
static struct tty_t tty_table[NR_TTYS];
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

		for (i = 0; i < NR_TTYS; i++)
			if (current_task->tty == tty_table[i].dev)
				return &tty_table[i];

		return NULL;
	}

	/* current active tty */
	if (dev == DEV_TTY0)
		return current_tty >= 0 ? &tty_table[current_tty] : NULL;

	/* asked tty */
	if (minor(dev) > 0 && minor(dev) <= NR_TTYS)
		return &tty_table[minor(dev) - 1];

	return NULL;
}

/*
 * Open a TTY.
 */
static int tty_open(struct file_t *filp)
{
	/* set current task tty */
	if (major(filp->f_inode->i_rdev) == major(DEV_TTY0))
		current_task->tty = filp->f_inode->i_rdev;

	return 0;
}
#include <fcntl.h>
/*
 * Read TTY.
 */
static int tty_read(struct file_t *filp, char *buf, int n)
{
	struct tty_t *tty;
	int count = 0;
	uint8_t key;

	/* get tty */
	tty = tty_lookup(filp->f_inode->i_rdev);
	if (!tty)
		return -EINVAL;

	/* read all characters */
	while (count < n) {
		/* non blocking mode : returns if no characters in cooked queue */
		if ((filp->f_flags & O_NONBLOCK) && ring_buffer_empty(&tty->cooked_queue))
			return -EAGAIN;

		/* read key */
		ring_buffer_read(&tty->cooked_queue, &key, 1);

		/* add key to buffer */
		((unsigned char *) buf)[count++] = key;

		/* end of line : return */
		if (L_CANON(tty) && key == '\n') {
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
		tty->write(tty);
	} else {
		ring_buffer_putc(&tty->write_queue, c);
		tty->write(tty);
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
	tty = tty_lookup(filp->f_inode->i_rdev);
	if (!tty)
		return -EINVAL;

	/* write not implemented */
	if (!tty->write)
		return -EINVAL;

	/* put characters in write queue */
	for (i = 0; i < n; i++) {
		/* put next character */
		ring_buffer_write(&tty->write_queue, &((uint8_t *) buf)[i], 1);

		/* write to tty */
		if (ring_buffer_full(&tty->write_queue) || i == n - 1)
			tty->write(tty);
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
	fb = &tty_new->fb;
	fb->active = 1;
	if (tty_new->mode == KD_TEXT)
		fb->ops->update_region(fb, 0, fb->width * fb->height);

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
	tty = tty_lookup(filp->f_inode->i_rdev);
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
			if (tty->ioctl) {
				ret = tty->ioctl(tty, request, arg);
				if (ret != -ENOIOCTLCMD)
					return ret;
			}

			printf("Unknown ioctl request (%x) on device %x\n", request, filp->f_inode->i_rdev);
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
	tty = tty_lookup(filp->f_inode->i_rdev);
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
static int tty_init(struct tty_t *tty, int num, struct multiboot_tag_framebuffer *tag_fb)
{
	int ret;

	tty->dev = DEV_TTY0 + num;
	tty->pgrp = 0;
	tty->wait = NULL;
	tty->write = console_write;
	tty->ioctl = console_ioctl;
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
	ret = init_framebuffer(&tty->fb, tag_fb, tty->erase_char, 0);
	if (ret)
		return ret;

	/* set winsize */
	tty->winsize.ws_row = tty->fb.height - 1;
	tty->winsize.ws_col = tty->fb.width - 1;
	tty->winsize.ws_xpixel = 0;
	tty->winsize.ws_ypixel = 0;

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

	/* init each tty */
	for (i = 0; i < NR_TTYS; i++) {
		ret = tty_init(&tty_table[i], i + 1, tag_fb);
		if (ret)
			break;
	}

	/* set current tty to first tty */
	current_tty = 0;
	tty_table[current_tty].fb.active = 1;

	/* on error destroy ttys */
	if (ret)
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

