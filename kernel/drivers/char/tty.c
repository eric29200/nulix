#include <drivers/char/serial.h>
#include <drivers/char/keyboard.h>
#include <drivers/char/termios.h>
#include <drivers/char/tty.h>
#include <drivers/char/pit.h>
#include <proc/sched.h>
#include <ipc/signal.h>
#include <stdio.h>
#include <stderr.h>
#include <ctype.h>
#include <fcntl.h>
#include <time.h>
#include <dev.h>
#include <kd.h>

#define DEV_TTY0		mkdev(DEV_TTY_MAJOR, 0)
#define DEV_TTY			mkdev(DEV_TTYAUX_MAJOR, 0)
#define DEV_CONSOLE		mkdev(DEV_TTYAUX_MAJOR, 1)
#define DEV_PTMX		mkdev(DEV_TTYAUX_MAJOR, 2)

/* global ttys table */
struct tty tty_table[NR_TTYS];

/*
 * Lookup for a tty.
 */
static struct tty *tty_lookup(dev_t dev)
{
	/* current task tty */
	if (dev == DEV_TTY)
		return current_task->tty;

 	/* console = always first tty */
	if (dev == DEV_CONSOLE)
		return &tty_table[0];

	/* current active console */
	if (dev == DEV_TTY0)
		return fg_console >= 0 ? &tty_table[fg_console] : NULL;

	/* console */
	if (major(dev) == major(DEV_TTY0) && minor(dev) > 0 && minor(dev) <= NR_CONSOLES)
		return &tty_table[minor(dev) - 1];

	/* pty */
	if (major(dev) == DEV_PTS_MAJOR && minor(dev) < NR_PTYS)
		return &tty_table[NR_CONSOLES + minor(dev)];

	return NULL;
}

/*
 * Open a TTY.
 */
static int tty_open(struct file *filp)
{
	struct dentry *dentry;
	struct inode *inode;
	struct tty *tty;
	int noctty;
	dev_t dev;

	/* get dentry */
	dentry = filp->f_dentry;
	if (!dentry)
		return -EINVAL;

	/* get inode */
	inode = dentry->d_inode;
	if (!inode)
		return -EINVAL;

	/* get tty device number */
	dev = inode->i_rdev;

	/* open pty controller */
	if (filp->f_dentry->d_inode->i_rdev == DEV_PTMX)
		return ptmx_open(filp);

	/* get tty */
	tty = tty_lookup(inode->i_rdev);
	if (!tty)
		return -EINVAL;

	/* attach tty to file */
	filp->f_private = tty;

	/* check if tty must be associated to process */
	noctty = filp->f_flags & O_NOCTTY;
	if (dev == DEV_TTY || dev == DEV_TTY0 || dev == DEV_PTMX)
		noctty = 1;

	/* associate tty */
	if (!noctty && current_task->leader) {
		current_task->tty = tty;
		tty->session = current_task->session;
		tty->pgrp = current_task->pgrp;
	}

	/* update reference count */
	tty->count++;

	return 0;
}

/*
 * Check tty count.
 */
static int tty_check_count(struct tty *tty)
{
	int count = 0, i;

	for (i = 0; i < NR_FILE; i++)
		if (filp_table[i].f_count && filp_table[i].f_private == tty)
			count++;

	return count;
}


/*
 * Close a TTY.
 */
static int tty_close(struct file *filp)
{
	struct tty *tty = filp->f_private;

	/* specifice close */
	if (tty->driver && tty->driver->close)
		return tty->driver->close(tty);

	/* reset termios on last tty release */
	if (!tty_check_count(tty))
		tty->termios = tty->driver->termios;

	/* update reference count */
	tty->count--;

	return 0;
}

/*
 * Read TTY.
 */
static int tty_read(struct file *filp, char *buf, int n)
{
	int count = 0, ret;
	struct tty *tty;
	uint8_t c;

	/* get tty */
	tty = filp->f_private;
	if (!tty)
		return -EINVAL;

	/* read all characters */
	while (count < n) {
		/* read next character */
		ret = ring_buffer_getc(&tty->cooked_queue, &c, filp->f_flags & O_NONBLOCK);
		if (ret)
			return ret;

		/* add char to buffer */
		((uint8_t *) buf)[count++] = c;

		/* end of line : return */
		if (L_CANON(tty) && c == '\n') {
			tty->canon_data--;
			break;
		}

		/* no more characters : break */
		if (!L_CANON(tty) && ring_buffer_empty(&tty->cooked_queue))
			break;
	}

	return count;
}

/*
 * Post a character to tty.
 */
static int opost(struct tty *tty, uint8_t c)
{
	int space;

	/* write queue is full */
	space = ring_buffer_left(&tty->write_queue);
	if (!space)
		return -EINVAL;

	/* handle special characters */
	if (O_POST(tty)) {
		switch (c) {
			case '\n':
				if (O_NLCR(tty)) {
					if (space < 2)
						return -EINVAL;

					ring_buffer_putc(&tty->write_queue, '\r');
				}
				break;
			case '\r':
				if (O_NOCR(tty))
					return 0;
				if (O_CRNL(tty))
					c = '\n';
				break;
			default:
				if (O_LCUC(tty))
					c = TOUPPER(c);
				break;
		}
	}

	/* post character */
	ring_buffer_putc(&tty->write_queue, c);
	return 0;
}

/*
 * Output/Echo a character.
 */
static void out_char(struct tty *tty, uint8_t c)
{
	if (ISCNTRL(c) && !ISSPACE(c) && L_ECHOCTL(tty)) {
		opost(tty, '^');
		opost(tty, c + 64);
		tty->driver->write(tty);
	} else {
		opost(tty, c);
		tty->driver->write(tty);
	}
}

/*
 * Cook input characters.
 */
void tty_do_cook(struct tty *tty)
{
	uint8_t c;

	for (;;) {
		/* get next input character */
		if (ring_buffer_getc(&tty->read_queue, &c, 1))
			break;

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
		ring_buffer_putc(&tty->cooked_queue, c);

		/* update canon data */
		if (L_CANON(tty) && c == '\n')
			tty->canon_data++;
	}

	/* wake up eventual process */
	wake_up(&tty->wait);
}

/*
 * Write to TTY.
 */
static int tty_write(struct file *filp, const char *buf, int n)
{
	struct tty *tty;
	int i;

	/* get tty */
	tty = filp->f_private;
	if (!tty)
		return -EINVAL;

	/* write not implemented */
	if (!tty->driver->write)
		return -EINVAL;

	/* pos characters */
	for (i = 0; i < n; i++) {
		/* put next character */
		opost(tty, buf[i]);

		/* write to tty */
		if (ring_buffer_full(&tty->write_queue) || i == n - 1)
			tty->driver->write(tty);
	}

	return n;
}

/*
 * Set tty's session.
 */
static int tiocsctty(struct tty *tty)
{
	struct list_head *pos;
	struct task *task;

	/* nothing to do */
	if (current_task->leader && current_task->session == tty->session)
		return 0;

	/* check permissions */
	if (!current_task->leader || current_task->tty)
		return -EPERM;

	/* this tty is already the controlling tty for another session group */
	if (tty->session > 0) {
		list_for_each(pos, &tasks_list) {
			task = list_entry(pos, struct task, list);
			if (task->tty == tty)
				task->tty = NULL;
		}
	}

	/* set session and pgrp */
	current_task->tty = tty;
	tty->session = current_task->session;
	tty->pgrp = current_task->pgrp;

	return 0;
}

/*
 * Flush tty input.
 */
static void tty_flush_input(struct tty *tty)
{
	ring_buffer_flush(&tty->read_queue);
	ring_buffer_flush(&tty->cooked_queue);
	tty->canon_data = 0;

	if (tty->link)
		ring_buffer_flush(&tty->link->write_queue);
}

/*
 * Flush tty output.
 */
static void tty_flush_output(struct tty *tty)
{
	ring_buffer_flush(&tty->write_queue);

	if (tty->link) {
		ring_buffer_flush(&tty->link->read_queue);
		ring_buffer_flush(&tty->link->cooked_queue);
		tty->canon_data = 0;
	}
}

/*
 * Disassociate current task tty.
 */
void disassociate_ctty()
{
	struct tty *tty = current_task->tty;
	struct list_head *pos;
	struct task *task;

	if (!tty)
		return;

	/* send SIGHUP/SIGCONT to fg process group */
	if (tty->pgrp > 0) {
		task_signal_group(tty->pgrp, SIGHUP);
		task_signal_group(tty->pgrp, SIGCONT);
	}

	/* clear tty */
	tty->session = 0;
	tty->pgrp = -1;

	/* clear tty for all processes in the session group */
	list_for_each(pos, &tasks_list) {
		task = list_entry(pos, struct task, list);
		if (task->session == current_task->session)
			task->tty = NULL;
	}
}

/*
 * TTY ioctl.
 */
int tty_ioctl(struct file *filp, int request, unsigned long arg)
{
	struct tty *tty;
	int ret;

	/* get tty */
	tty = filp->f_private;
	if (!tty)
		return -EINVAL;

	switch (request) {
		case TCGETS:
			memcpy((struct termios *) arg, &tty->termios, sizeof(struct termios));
			break;
		case TCSETS:
		case TCSETSW:
		case TCSETSF:
			memcpy(&tty->termios, (struct termios *) arg, sizeof(struct termios));
			tty->canon_data = 0;
			break;
		case TIOCGWINSZ:
			memcpy((struct winsize *) arg, &tty->winsize, sizeof(struct winsize));
			break;
		case TIOCSWINSZ:
			memcpy(&tty->winsize, (struct winsize *) arg, sizeof(struct winsize));
			break;
		case TIOCGPGRP:
			*((pid_t *) arg) = tty->pgrp;
			break;
		case TIOCSPGRP:
			if (current_task->session != tty->session)
				return -ENOTTY;

			tty->pgrp = *((pid_t *) arg);
			break;
		case TIOCGSID:
			*((pid_t *) arg) = tty->session;
			break;
		case TIOCSCTTY:
		   	return tiocsctty(tty);
		case TIOCGLCKTRMIOS:
		case TIOCSLCKTRMIOS:
			break;
		case TCFLSH:
			switch (arg) {
				case TCIFLUSH:
				 	tty_flush_input(tty);
					break;
				case TCOFLUSH:
				 	tty_flush_output(tty);
					break;
				case TCIOFLUSH:
				 	tty_flush_input(tty);
				 	tty_flush_output(tty);
					break;
				default:
					return -EINVAL;
			}

			break;
		case TIOCNOTTY:
			if (current_task->tty != tty)
				return -ENOTTY;
			if (current_task->leader)
				disassociate_ctty();

			current_task->tty = NULL;
			return 0;
		default:
			if (tty->driver->ioctl) {
				ret = tty->driver->ioctl(tty, request, arg);
				if (ret != -ENOIOCTLCMD)
					return ret;
			}

			printf("Unknown ioctl request (0x%x) on device 0x%x\n", request, (int) filp->f_dentry->d_inode->i_rdev);
			break;
	}

	return 0;
}

/*
 * Check if there is some data to read.
 */
static int tty_input_available(struct tty *tty)
{
	if (L_CANON(tty))
		return tty->canon_data > 0;

	return tty->cooked_queue.size > 0;
}

/*
 * Poll a tty.
 */
static int tty_poll(struct file *filp, struct select_table *wait)
{
	struct tty *tty;
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
 * Init a tty.
 */
int tty_init_dev(struct tty *tty, dev_t device, struct tty_driver *driver)
{
	int ret;

	memset(tty, 0, sizeof(struct tty));
	tty->driver = driver;
	tty->device = device;

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

	/* init termios */
	tty->termios = driver->termios;

	return 0;
}

/*
 * Destroy a tty.
 */
void tty_destroy(struct tty *tty)
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
	int ret;

	/* reset ttys */
	memset(tty_table, 0, sizeof(struct tty) * NR_TTYS);

	/* init ptys */
	ret = init_pty();
	if (ret)
		return ret;

	/* init consoles */
	ret = init_console(tag_fb);
	if (ret)
		return ret;

	return 0;
}

/*
 * Tty file operations.
 */
static struct file_operations tty_fops = {
	.open		= tty_open,
	.close		= tty_close,
	.read		= tty_read,
	.write		= tty_write,
	.poll		= tty_poll,
	.ioctl		= tty_ioctl,
};

/*
 * Tty inode operations.
 */
struct inode_operations tty_iops = {
	.fops		= &tty_fops,
};
