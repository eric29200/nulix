#include <drivers/serial.h>
#include <drivers/keyboard.h>
#include <drivers/termios.h>
#include <drivers/tty.h>
#include <drivers/console.h>
#include <drivers/pit.h>
#include <proc/sched.h>
#include <ipc/signal.h>
#include <stdio.h>
#include <stderr.h>
#include <time.h>
#include <dev.h>

#define NB_TTYS		4

/* global ttys */
static struct tty_t tty_table[NB_TTYS];
static int current_tty;

/*
 * Lookup for a tty.
 */
struct tty_t *tty_lookup(dev_t dev)
{
	int i;

	/* current task tty */
	if (dev == DEV_TTY) {
		for (i = 0; i < NB_TTYS; i++)
			if (current_task->tty == tty_table[i].dev)
				return &tty_table[i];

		return NULL;
	}

	/* current active tty */
	if (dev == DEV_TTY0)
		return current_tty >= 0 ? &tty_table[current_tty] : NULL;

	/* asked tty */
	if (minor(dev) > 0 && minor(dev) <= NB_TTYS)
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
		/* read key */
		ring_buffer_read(&tty->buffer, &key, 1);

		/* add key to buffer */
		((unsigned char *) buf)[count++] = key;

		/* end of line : return */
		if (key == '\n')
			break;
	}

	return count;
}

/*
 * Write a character to tty.
 */
void tty_update(unsigned char c)
{
	struct tty_t *tty;
	uint8_t buf[8];
	int len;

	/* no current TTY */
	if (current_tty < 0)
		return;

	/* get tty */
	tty = &tty_table[current_tty];

	/* set buffer */
	len = 0;

	/* handle special keys */
	switch (c) {
		case KEY_PAGEUP:
			buf[0] = 27;
			buf[1] = 91;
			buf[2] = 53;
			buf[3] = 126;
			len = 4;
			break;
		case KEY_PAGEDOWN:
			buf[0] = 27;
			buf[1] = 91;
			buf[2] = 54;
			buf[3] = 126;
			len = 4;
			break;
		case KEY_HOME:
			buf[1] = 27;
			buf[2] = 91;
			buf[3] = 72;
			len = 3;
			break;
		case KEY_END:
			buf[0] = 27;
			buf[1] = 91;
			buf[2] = 70;
			len = 3;
			break;
		case KEY_INSERT:
			buf[0] = 27;
			buf[1] = 91;
			buf[2] = 50;
			buf[3] = 126;
			len = 4;
			break;
		case KEY_DELETE:
			buf[0] = 27;
			buf[1] = 91;
			buf[2] = 51;
			buf[3] = 126;
			len = 4;
			break;
		case KEY_UP:
			buf[0] = 27;
			buf[1] = 91;
			buf[2] = 65;
			len = 3;
			break;
		case KEY_DOWN:
			buf[0] = 27;
			buf[1] = 91;
			buf[2] = 66;
			len = 3;
			break;
		case KEY_RIGHT:
			buf[0] = 27;
			buf[1] = 91;
			buf[2] = 67;
			len = 3;
			break;
		case KEY_LEFT:
			buf[0] = 27;
			buf[1] = 91;
			buf[2] = 68;
			len = 3;
			break;
		case 13:
			buf[0] = '\n';
			len = 1;
			break;
		default:
			buf[0] = c;
			len = 1;
			break;
	}

	/* store buffer */
	if (len > 0)
		ring_buffer_write(&tty->buffer, buf, len);

	/* echo character on device */
	if (L_ECHO(tty) && len > 0)
		tty->write(tty, (char *) buf, len);

	/* wake up eventual process */
	task_wakeup(tty);
}

/*
 * Write to TTY.
 */
static int tty_write(struct file_t *filp, const char *buf, int n)
{
	struct tty_t *tty;

	/* get tty */
	tty = tty_lookup(filp->f_inode->i_rdev);
	if (!tty)
		return -EINVAL;

	/* write not implemented */
	if (!tty->write)
		return -EINVAL;

	return tty->write(tty, buf, n);
}

/*
 * Change current tty.
 */
void tty_change(int n)
{
	struct framebuffer_t *fb;

	if (n >= 0 && n < NB_TTYS) {

		/* refresh frame buffer */
		if (current_tty != n) {
			fb = &tty_table[n].fb;
			fb->update_region(fb, 0, fb->width * fb->height);
		}

		current_tty = n;
	}
}

/*
 * TTY ioctl.
 */
int tty_ioctl(struct file_t *filp, int request, unsigned long arg)
{
	struct tty_t *tty;

	/* get tty */
	tty = tty_lookup(filp->f_inode->i_rdev);
	if (!tty)
		return -EINVAL;

	switch (request) {
		case TCGETS:
			memcpy((struct termios_t *) arg, &tty->termios, sizeof(struct termios_t));
			break;
		case TCSETS:
			memcpy(&tty->termios, (struct termios_t *) arg, sizeof(struct termios_t));
			break;
		case TCSETSW:
			memcpy(&tty->termios, (struct termios_t *) arg, sizeof(struct termios_t));
			break;
		case TCSETSF:
			memcpy(&tty->termios, (struct termios_t *) arg, sizeof(struct termios_t));
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
			printf("Unknown ioctl request (%x) on device %x\n", request, filp->f_inode->i_rdev);
			break;
	}

	return 0;
}

/*
 * Poll a tty.
 */
static int tty_poll(struct file_t *filp)
{
	struct tty_t *tty;
	int mask = 0;

	/* get tty */
	tty = tty_lookup(filp->f_inode->i_rdev);
	if (!tty)
		return -EINVAL;

	/* set waiting channel */
	current_task->waiting_chan = tty;

	/* check if there is some characters to read */
	if (tty->buffer.size > 0)
		mask |= POLLIN;

	return mask;
}

/*
 * Signal foreground processes group.
 */
void tty_signal_group(dev_t dev, int sig)
{
	struct tty_t *tty;

	/* get tty */
	tty = tty_lookup(dev);
	if (!tty)
		return;

	/* send signal */
	task_signal_group(tty->pgrp, sig);
}

/*
 * Default tty attributes.
 */
void tty_default_attr(struct tty_t *tty)
{
	tty->color_bg = TEXT_BLACK;
	tty->color = TEXT_COLOR(tty->color_bg, TEXT_LIGHT_GREY);
	tty->erase_char = ' ' | (tty->color << 8);
	tty->deccm = 1;
}

/*
 * Init TTYs.
 */
int init_tty(struct multiboot_tag_framebuffer *tag_fb)
{
	int i, ret;

	/* init each tty */
	for (i = 0; i < NB_TTYS; i++) {
		tty_table[i].dev = DEV_TTY0 + i + 1;
		tty_table[i].pgrp = 0;
		tty_table[i].write = console_write;
		tty_default_attr(&tty_table[i]);

		/* init buffer */
		ret = ring_buffer_init(&tty_table[i].buffer, TTY_BUF_SIZE);
		if (ret != 0)
			return ret;

		/* init frame buffer */
		ret = init_framebuffer(&tty_table[i].fb, tag_fb, tty_table[i].erase_char);
		if (ret != 0)
			return ret;

		/* set winsize */
		tty_table[i].winsize.ws_row = tty_table[i].fb.height;
		tty_table[i].winsize.ws_col = tty_table[i].fb.width;
		tty_table[i].winsize.ws_xpixel = 0;
		tty_table[i].winsize.ws_ypixel = 0;

		/* init termios */
		tty_table[i].termios = (struct termios_t) {
			.c_iflag	= ICRNL,
			.c_oflag	= OPOST | ONLCR,
			.c_cflag	= 0,
			.c_lflag	= IXON | ISIG | ICANON | ECHO | ECHOCTL | ECHOKE,
			.c_line		= 0,
			.c_cc		= INIT_C_CC,
		};
	}

	/* set current tty to first tty */
	current_tty = 0;

	return 0;
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

