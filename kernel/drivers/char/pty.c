#include <drivers/char/tty.h>
#include <proc/sched.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <dev.h>

#define PTY_NAME_LEN		64

/* pty table */
static struct pty pty_table[NR_PTYS];

/* Pty master/slave operations (set on init_pty) */
static struct file_operations ptm_fops;
static struct file_operations pts_fops;
struct inode_operations ptm_iops;
struct inode_operations pts_iops;

/*
 * Master/slave pty write.
 */
static ssize_t pty_write(struct tty *tty)
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
static struct tty_driver pts_driver = {
	.write		= pty_write,
	.termios 	= (struct termios) {
				.c_iflag	= ICRNL | IXON,
				.c_oflag	= OPOST | ONLCR,
				.c_cflag	= B38400 | CS8 | CREAD | HUPCL,
				.c_lflag	= ISIG | ICANON | ECHO | ECHOE | ECHOK | ECHOCTL | ECHOKE | IEXTEN,
				.c_line		= 0,
				.c_cc		= INIT_C_CC,
			},
};

/*
 * Master pty ioctl.
 */
static int ptm_ioctl(struct tty *tty, int request, unsigned long arg)
{
	struct pty *pty = tty->driver_data;

	switch (request) {
		case TIOCGPTN:
			*((int *) arg) = pty->p_num;
			return 0;
		case TIOCSPTLCK:
			return 0;
		default:
			break;
	}

	return -ENOIOCTLCMD;
}

/*
 * Master pty close.
 */
static int ptm_close(struct tty *tty)
{
	struct pty *pty = tty->driver_data;
	struct list_head *pos;
	struct task *task;

	if (!tty->link)
		return 0;

	/* unregister device */
	if (pty->de)
		devpts_unregister(pty->de);

	/* update reference count */
	pty->p_count--;

	/* send SIGHUP signal to processes attached to slave pty */
	list_for_each(pos, &current_task->list) {
		task = list_entry(pos, struct task, list);
		if (task->tty == tty->link) {
			task_signal(task->pid, SIGHUP);
			task_signal(task->pid, SIGCONT);
		}
	}

	return 0;
}

/*
 * Master pty driver.
 */
static struct tty_driver ptm_driver = {
	.write		= pty_write,
	.ioctl		= ptm_ioctl,
	.close		= ptm_close,
};

/*
 * Open PTY master = create a new pty master/slave.
 */
static int ptmx_open(struct file *filp)
{
	struct tty *ptm = NULL, *pts = NULL;
	char name[PTY_NAME_LEN];
	int ret, i;

	/* find a free pty */
	for (i = 0; i < NR_PTYS; i++)
		if (pty_table[i].p_count == 0)
			break;

	/* no free pty : exit */
	if (i >= NR_PTYS)
		return -ENOMEM;

	/* create slave pty */
	pts = &tty_table[NR_CONSOLES + i];
	memset(pts, 0, sizeof(struct tty));
	ret = tty_init_dev(pts, &pts_driver);
	if (ret)
		goto err;

	/* create master pty */
	ptm = &tty_table[NR_CONSOLES + NR_PTYS + i];
	memset(ptm, 0, sizeof(struct tty));
	ret = tty_init_dev(ptm, &ptm_driver);
	if (ret)
		goto err;

	/* reset master termios */
	memset(&ptm->termios, 0, sizeof(struct termios));

	/* create slave pty node */
	sprintf(name, "%d", i);
	pty_table[i].de = devpts_register(name, S_IFCHR | 0660, mkdev(DEV_PTS_MAJOR, i));
	if (!pty_table[i].de) {
		ret = -ENOSPC;
		goto err;
	}

	/* attach pty struct to master/slave */
	ptm->driver_data = &pty_table[i];
	pts->driver_data = &pty_table[i];

	/* set links */
	ptm->link = pts;
	pts->link = ptm;

	/* attach master pty to file */
	filp->f_private = ptm;

	/* update pty reference count */
	pty_table[i].p_count++;

	return 0;
err:
	/* free device */
	if (ptm)
		memset(ptm, 0, sizeof(struct tty));
	if (pts)
		memset(pts, 0, sizeof(struct tty));
	return ret;
}

/*
 * Init ptys.
 */
int init_pty()
{
	int i;

	/* set pty table */
	for (i = 0; i < NR_PTYS; i++) {
		pty_table[i].p_num = i;
		pty_table[i].p_count = 0;
	}

	/* install master pty operations */
	ptm_iops.fops = &ptm_fops;
	ptm_fops = *tty_iops.fops;
	ptm_fops.open = ptmx_open;

	/* install slave pty operations */
	pts_iops.fops = &pts_fops;
	pts_fops = *tty_iops.fops;

	return 0;
}
