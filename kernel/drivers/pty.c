#include <drivers/tty.h>
#include <proc/sched.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <dev.h>

#define PTY_NAME_LEN		64

/* Pty master/slave operations (set on init_pty) */
static struct file_operations_t ptm_fops;
static struct file_operations_t pts_fops;
struct inode_operations_t ptm_iops;
struct inode_operations_t pts_iops;

/*
 * Master/slave pty write.
 */
static ssize_t pty_write(struct tty_t *tty)
{
	ssize_t count = 0;
	uint8_t c;

	/* get characters from write queue */
	while (tty->write_queue.size > 0) {
		/* get next character */
		ring_buffer_read(&tty->write_queue, &c, 1);

		/* write to linked tty */
		if (ring_buffer_putc(&tty->link->read_queue, c))
			break;

		count++;
	}

	/* do cook */
	tty_do_cook(tty->link);
	return count;
}

/*
 * Slave pty driver.
 */
static struct tty_driver_t pts_driver = {
	.write		= pty_write,
};

/*
 * Pty master ioctl.
 */
static int ptm_ioctl(struct tty_t *tty, int request, unsigned long arg)
{
	switch (request) {
		case TIOCGPTN:
			*((int *) arg) = tty->link->dev - mkdev(DEV_PTS_MAJOR, 0);
			return 0;
		case TIOCSPTLCK:
			return 0;
		default:
			break;
	}

	return -ENOIOCTLCMD;
}

/*
 * Master pty driver.
 */
static struct tty_driver_t ptm_driver = {
	.write		= pty_write,
	.ioctl		= ptm_ioctl,
};

/*
 * Open PTY master = create a new pty master/slave.
 */
static int ptmx_open(struct file_t *filp)
{
	struct tty_t *ptm = NULL, *pts = NULL;
	char name[PTY_NAME_LEN];
	int ret, i;

	/* find a free slave pty */
	for (i = 0; i < NR_PTYS; i++)
		if (pty_table[i].dev == 0)
			break;

	/* no free pty : exit */
	if (i >= NR_PTYS)
		return -ENOMEM;

	/* create master pty */
	ptm = &pty_table[NR_PTYS + i];
	memset(ptm, 0, sizeof(struct tty_t));
	ret = tty_init_dev(ptm, DEV_PTMX, &ptm_driver, NULL);
	if (ret)
		goto err;

	/* create slave pty */
	pts = &pty_table[i];
	memset(pts, 0, sizeof(struct tty_t));
	ret = tty_init_dev(pts, mkdev(DEV_PTS_MAJOR, i), &pts_driver, NULL);
	if (ret)
		goto err;

	/* reset master termios */
	memset(&ptm->termios, 0, sizeof(struct termios_t));

	/* create slave pty node */
	sprintf(name, "/dev/pts/%d", i);
	ret = do_mknod(AT_FDCWD, name, S_IFCHR | S_IRWXUGO, mkdev(DEV_PTS_MAJOR, i));
	if (ret)
		goto err;

	/* set links */
	ptm->link = pts;
	pts->link = ptm;

	/* attach master pty to file */
	filp->f_private = ptm;

	return 0;
err:
	/* free device */
	if (ptm)
		memset(ptm, 0, sizeof(struct tty_t));
	if (pts)
		memset(pts, 0, sizeof(struct tty_t));
	return ret;
}

/*
 * Init ptys.
 */
void init_pty()
{
	/* memzero pty table */
	memset(&pty_table, 0, sizeof(struct tty_t) * NR_PTYS * 2);

	/* install master pty operations */
	ptm_iops.fops = &ptm_fops;
	ptm_fops = *tty_iops.fops;
	ptm_fops.open = ptmx_open;

	/* install slave pty operations */
	pts_iops.fops = &pts_fops;
	pts_fops = *tty_iops.fops;
}
