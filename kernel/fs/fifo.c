#include <fs/fs.h>
#include <proc/sched.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Open a fifo.
 */
static int fifo_open(struct inode *inode, struct file *filp)
{
	int ret = 0;

	switch (filp->f_mode) {
		case 1:
			filp->f_op = &connecting_fifo_fops;

			/* update number of readers */
			if (!PIPE_READERS(inode)++)
				wake_up(&PIPE_WAIT(inode));

			/* wait for writers */
			if (!(filp->f_flags & O_NONBLOCK) && !PIPE_WRITERS(inode)) {
				PIPE_RD_OPENERS(inode)++;

				while (!PIPE_WRITERS(inode)) {
					if (signal_pending(current_task)) {
						ret = -EINTR;
						break;
					}

					sleep_on(&PIPE_WAIT(inode));
				}

				if (!--PIPE_RD_OPENERS(inode))
					wake_up(&PIPE_WAIT(inode));
			}

			/* wait for opening writers end */
			while (PIPE_WR_OPENERS(inode))
				sleep_on(&PIPE_WAIT(inode));

			/* connected */
			if (PIPE_WRITERS(inode))
				filp->f_op = &read_fifo_fops;

			/* handle error */
			if (ret && !--PIPE_READERS(inode))
				wake_up(&PIPE_WAIT(inode));

			break;
		case 2:
			/* no readers */
			if ((filp->f_flags & O_NONBLOCK) && !PIPE_READERS(inode)) {
				ret = -ENXIO;
				break;
			}

			/* set write mode */
			filp->f_op = &write_fifo_fops;
			if (!PIPE_WRITERS(inode)++)
				wake_up(&PIPE_WAIT(inode));

			/* wait for readers */
			if (!PIPE_READERS(inode)) {
				PIPE_WR_OPENERS(inode)++;

				while (!PIPE_READERS(inode)) {
					if (signal_pending(current_task)) {
						ret = -EINTR;
						break;
					}

					sleep_on(&PIPE_WAIT(inode));
				}

				if (!--PIPE_WR_OPENERS(inode))
					wake_up(&PIPE_WAIT(inode));
			}

			while (PIPE_RD_OPENERS(inode))
				sleep_on(&PIPE_WAIT(inode));

			/* handle error */
			if (ret && !--PIPE_WRITERS(inode))
				wake_up(&PIPE_WAIT(inode));

			break;
		case 3:
			filp->f_op = &rdwr_fifo_fops;

			/* wait for other openers */
			if (!PIPE_READERS(inode)++)
				wake_up(&PIPE_WAIT(inode));
			while (PIPE_WR_OPENERS(inode))
				sleep_on(&PIPE_WAIT(inode));
			if (!PIPE_WRITERS(inode)++)
				wake_up(&PIPE_WAIT(inode));
			while (PIPE_RD_OPENERS(inode))
				sleep_on(&PIPE_WAIT(inode));

			break;
		default:
			ret = -EINVAL;
			break;
	}

	if (ret || PIPE_BASE(inode))
		return ret;

	/* set pipe inode */
	PIPE_BASE(inode) = get_free_page();
	if (!PIPE_BASE(inode))
		return -ENOMEM;

	PIPE_START(inode) = 0;
	PIPE_LEN(inode) = 0;

	return 0;
}

/*
 * Fifo file operations.
 */
static struct file_operations fifo_fops = {
	.open		= fifo_open,
};

/*
 * Fifo inode operations.
 */
static struct inode_operations fifo_iops = {
	.fops		= &fifo_fops,
};

/*
 * Init a fifo inode.
 */
void init_fifo(struct inode *inode)
{
	inode->i_op = &fifo_iops;
	PIPE_BASE(inode) = NULL;
	PIPE_START(inode) = 0;
	PIPE_LEN(inode) = 0;
	PIPE_RD_OPENERS(inode) = 0;
	PIPE_WR_OPENERS(inode) = 0;
	PIPE_READERS(inode) = 0;
	PIPE_WRITERS(inode) = 0;
	PIPE_WAIT(inode) = NULL;
}