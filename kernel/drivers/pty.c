#include <drivers/pty.h>
#include <drivers/tty.h>
#include <drivers/termios.h>
#include <proc/sched.h>
#include <fs/fs.h>
#include <dev.h>
#include <stdio.h>
#include <stderr.h>

#define NB_PTYS		4

/* global ptys */
static struct tty_t pty_table[NB_PTYS];
static int current_pty = 1;

/*
 * Open ptys multiplexer = create a new pty.
 */
static int ptmx_open(struct file_t *filp)
{
	/* unused file */
	UNUSED(filp);

	/* update current pty */
	current_pty++;
	if (current_pty > NB_PTYS)
		current_pty = 1;

	return 0;
}

/*
 * PTY multiplexer ioctl.
 */
int ptmx_ioctl(struct file_t *filp, int request, unsigned long arg)
{
	UNUSED(filp);

	switch (request) {
		case TIOCGPTN:
			*((uint32_t *) arg) = current_pty;
			break;
		default:
			printf("Unknown ioctl request (%x) on device %x\n", request, DEV_PTMX);
			break;
	}

	return 0;
}

/*
 * Lookup for a pty.
 */
static struct tty_t *pty_lookup(dev_t dev)
{
	if (minor(dev) > 0 && minor(dev) <= NB_PTYS)
		return &pty_table[minor(dev) - 1];

	return NULL;
}

/*
 * Read PTY.
 */
static int pty_read(struct file_t *filp, char *buf, int n)
{
	struct tty_t *pty;
	int count = 0;
	uint8_t key;

	/* get pty */
	pty = pty_lookup(filp->f_inode->i_cdev);
	if (!pty)
		return -EINVAL;

	/* read all characters */
	while (count < n) {
		/* read key */
		ring_buffer_read(&pty->buffer, &key, 1);

		/* add key to buffer */
		((unsigned char *) buf)[count++] = key;

		/* end of line : return */
		if (key == '\n')
			break;
	}

	return count;
}

/*
 * Write to PTY.
 */
static int pty_write(struct file_t *filp, const char *buf, int n)
{
	struct tty_t *pty;

	/* get pty */
	pty = pty_lookup(filp->f_inode->i_cdev);
	if (!pty)
		return -EINVAL;

	/* write to buffer */
	return ring_buffer_write(&pty->buffer, (const uint8_t *) buf, n);
}

/*
 * Poll a pty.
 */
static int pty_poll(struct file_t *filp)
{
	struct tty_t *pty;
	int mask = 0;

	/* get pty */
	pty = pty_lookup(filp->f_inode->i_cdev);
	if (!pty)
		return -EINVAL;

	/* set waiting channel */
	current_task->waiting_chan = pty;

	/* check if there is some characters to read */
	if (pty->buffer.size > 0)
		mask |= POLLIN;

	return mask;
}

/*
 * PTY ioctl.
 */
int pty_ioctl(struct file_t *filp, int request, unsigned long arg)
{
	struct tty_t *pty;

	/* get tty */
	pty = pty_lookup(filp->f_inode->i_cdev);
	if (!pty)
		return -EINVAL;

	switch (request) {
		case TCGETS:
			memcpy((struct termios_t *) arg, &pty->termios, sizeof(struct termios_t));
			break;
		case TCSETS:
			memcpy(&pty->termios, (struct termios_t *) arg, sizeof(struct termios_t));
			break;
		case TCSETSW:
			memcpy(&pty->termios, (struct termios_t *) arg, sizeof(struct termios_t));
			break;
		case TCSETSF:
			memcpy(&pty->termios, (struct termios_t *) arg, sizeof(struct termios_t));
			break;
		case TIOCGWINSZ:
			memcpy((struct winsize_t *) arg, &pty->winsize, sizeof(struct winsize_t));
			break;
		case TIOCGPGRP:
			*((pid_t *) arg) = pty->pgrp;
			break;
		case TIOCSPGRP:
			pty->pgrp = *((pid_t *) arg);
			break;
		default:
			printf("Unknown ioctl request (%x) on device %x\n", request, filp->f_inode->i_cdev);
			break;
	}

	return 0;
}

/*
 * Init PTYs.
 */
int init_pty()
{
	int i, ret;

	/* init each tty */
	for (i = 0; i < NB_PTYS; i++) {
		memset(&pty_table[i], 0, sizeof(struct tty_t));
		pty_table[i].dev = mkdev(DEV_PTY_MAJOR, i + 1);
		pty_table[i].pgrp = 0;
		pty_table[i].write = NULL;

		/* init buffer */
		ret = ring_buffer_init(&pty_table[i].buffer, TTY_BUF_SIZE);
		if (ret != 0)
			return ret;

		/* init termios */
		pty_table[i].termios = (struct termios_t) {
			.c_iflag	= ICRNL,
			.c_oflag	= OPOST | ONLCR,
			.c_cflag	= 0,
			.c_lflag	= IXON | ISIG | ICANON | ECHO | ECHOCTL | ECHOKE,
			.c_line		= 0,
			.c_cc		= INIT_C_CC,
		};
	}

	return 0;
}

/*
 * Pty file operations.
 */
static struct file_operations_t pty_fops = {
	.read		= pty_read,
	.write		= pty_write,
	.poll		= pty_poll,
	.ioctl		= pty_ioctl,
};

/*
 * Pty inode operations.
 */
struct inode_operations_t pty_iops = {
	.fops		= &pty_fops,
};


/*
 * Ptmx file operations.
 */
static struct file_operations_t ptmx_fops = {
	.open		= ptmx_open,
	.ioctl		= ptmx_ioctl,
};

/*
 * Ptmx inode operations.
 */
struct inode_operations_t ptmx_iops = {
	.fops		= &ptmx_fops,
};

