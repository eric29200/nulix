#include <fs/fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <stderr.h>
#include <fcntl.h>

/* global file table (defined in open.c) */
extern struct file filp_table[NR_FILE];

/*
 * Read from a pipe.
 */
static int pipe_read(struct file *filp, char *buf, size_t count, off_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	size_t chars, size, read = 0;
	char *pipebuf;

	/* check position */
	if (ppos != &filp->f_pos)
		return -ESPIPE;

	/* nothing to read */
	if (!count)
		return 0;

	/* sleep while empty */
	if (filp->f_flags & O_NONBLOCK) {
		if (PIPE_EMPTY(inode)) {
			if (PIPE_WRITERS(inode))
				return -EAGAIN;
			else
				return 0;
		}
	} else {
		while (PIPE_EMPTY(inode)) {
			/* no writer : return */
			if (!PIPE_WRITERS(inode))
				return 0;

			/* process interruption */
			if (signal_pending(current_task))
				return -ERESTARTSYS;

			/* wait for some data */
			sleep_on(&PIPE_WAIT(inode));
		}
	}

	/* read available data */
	while (count > 0) {
		/* check size */
		size = PIPE_SIZE(inode);
		if (!size)
			break;

		/* compute number of characters to read */
		chars = PIPE_MAX_RCHUNK(inode);
		if (chars > count)
			chars = count;
		if (chars > size)
			chars = size;

		/* update size */
		count -= chars;
		read += chars;

		/* update pipe read position */
		pipebuf = PIPE_BASE(inode) + PIPE_START(inode);
		PIPE_START(inode) += chars;
		PIPE_START(inode) &= (PIPE_BUF - 1);
		PIPE_LEN(inode) -= chars;

		/* copy data to buffer */
		memcpy(buf, pipebuf, chars);
		buf += chars;
	}

	/* wake up writer */
	wake_up(&PIPE_WAIT(inode));

	if (read) {
		update_atime(inode);
		return read;
	}

	if (PIPE_WRITERS(inode))
		return -EAGAIN;

	return 0;
}

/*
 * Read from a connecting fifo.
 */
static int connect_read(struct file *filp, char *buf, size_t count, off_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;

	/* empty fifo */
	if (PIPE_EMPTY(inode) && !PIPE_WRITERS(inode))
		return 0;

	/* read from fifo */
	filp->f_op = &read_fifo_fops;
	return pipe_read(filp, buf, count, ppos);
}

/*
 * Write to a pipe.
 */
static int pipe_write(struct file *filp, const char *buf, size_t count, off_t *ppos)
{
	struct inode *inode = filp->f_dentry->d_inode;
	int ret = 0, written = 0;
	size_t chars, free;
	char *pipebuf;

	/* check position */
	if (ppos != &filp->f_pos)
		return -ESPIPE;

	/* nothing to write */
	if (!count)
		return 0;

	/* no readers */
	if (!PIPE_READERS(inode)) {
		send_sig(current_task, SIGPIPE, 0);
		return -ESPIPE;
	}

	while (count > 0) {
		/* no free space */
		while (!PIPE_EMPTY(inode)) {
			/* no readers */
			if (!PIPE_READERS(inode)) {
				send_sig(current_task, SIGPIPE, 0);
				ret = -EPIPE;
				goto out;
			}

			/* process interruption */
			if (signal_pending(current_task)) {
				ret = -ERESTARTSYS;
				goto out;
			}

			/* non blocking */
			if (filp->f_flags & O_NONBLOCK) {
				ret = -EAGAIN;
				goto out;
			}

			/* wait for free space */
			sleep_on(&PIPE_WAIT(inode));
		}

		while (count > 0) {
			/* check free size */
			free = PIPE_FREE(inode);
			if (!free)
				break;

			/* compute number of characters to write */
			chars = PIPE_MAX_WCHUNK(inode);
			if (chars > count)
				chars = count;
			if (chars > free)
				chars = free;

			/* update size */
			count -= chars;
			written += chars;

			/* update pipe read position */
			pipebuf = PIPE_BASE(inode) + PIPE_END(inode);
			PIPE_LEN(inode) += chars;

			/* copy data to buffer */
			memcpy(pipebuf, buf, chars);
			buf += chars;
		}

		/* wake up readers */
		wake_up(&PIPE_WAIT(inode));
		free = 1;
	}

out:
	inode->i_ctime = inode->i_mtime = CURRENT_TIME;
	mark_inode_dirty(inode);
	return written ? written : ret;
}

/*
 * Open a read pipe.
 */
static int pipe_read_open(struct inode *inode, struct file *filp)
{
	UNUSED(filp);
	PIPE_READERS(inode)++;
	return 0;
}

/*
 * Open a write pipe.
 */
static int pipe_write_open(struct inode *inode, struct file *filp)
{
	UNUSED(filp);
	PIPE_WRITERS(inode)++;
	return 0;
}

/*
 * Open a read/write pipe.
 */
static int pipe_rdwr_open(struct inode *inode, struct file *filp)
{
	if (filp->f_mode & FMODE_READ)
		PIPE_READERS(inode)++;

	if (filp->f_mode & FMODE_WRITE)
		PIPE_WRITERS(inode)++;

	return 0;
}

/*
 * Close a read pipe.
 */
static int pipe_read_release(struct inode *inode, struct file *filp)
{
	UNUSED(filp);

	PIPE_READERS(inode)--;
	if (!PIPE_READERS(inode) && !PIPE_WRITERS(inode))
		free_page(PIPE_BASE(inode));
	else
		wake_up(&PIPE_WAIT(inode));

	return 0;
}

/*
 * Close a write pipe.
 */
static int pipe_write_release(struct inode *inode, struct file *filp)
{
	UNUSED(filp);

	PIPE_WRITERS(inode)--;
	if (!PIPE_READERS(inode) && !PIPE_WRITERS(inode))
		free_page(PIPE_BASE(inode));
	else
		wake_up(&PIPE_WAIT(inode));

	return 0;
}

/*
 * Close a read/write pipe.
 */
static int pipe_rdwr_release(struct inode *inode, struct file *filp)
{
	if (filp->f_mode & FMODE_READ)
		PIPE_READERS(inode)--;

	if (filp->f_mode & FMODE_WRITE)
		PIPE_WRITERS(inode)--;

	return 0;
}

/*
 * Poll a pipe.
 */
static int pipe_poll(struct file *filp, struct select_table *wait)
{
	struct inode *inode = filp->f_dentry->d_inode;
	int mask = POLLIN;

	/* add wait queue to select table */
	select_wait(&PIPE_WAIT(inode), wait);

	if (PIPE_EMPTY(inode))
		mask = POLLOUT;
	if (!PIPE_WRITERS(inode))
		mask |= POLLHUP;
	if (!PIPE_READERS(inode))
		mask |= POLLERR;

	return mask;
}

/*
 * Poll a fifo.
 */
static int fifo_poll(struct file *filp, struct select_table *wait)
{
	struct inode *inode = filp->f_dentry->d_inode;
	int mask = POLLIN;

	/* add wait queue to select table */
	select_wait(&PIPE_WAIT(inode), wait);

	if (PIPE_EMPTY(inode))
		mask = POLLOUT;
	if (!PIPE_READERS(inode))
		mask |= POLLERR;

	return mask;
}

/*
 * Poll a connecting fifo.
 */
static int connect_poll(struct file *filp, struct select_table *wait)
{
	struct inode *inode = filp->f_dentry->d_inode;

	/* add wait queue to select table */
	select_wait(&PIPE_WAIT(filp->f_dentry->d_inode), wait);

	if (!PIPE_EMPTY(inode)) {
		filp->f_op = &read_fifo_fops;
		return POLLIN;
	}

	if (PIPE_WRITERS(inode))
		filp->f_op = &read_fifo_fops;

	return POLLOUT;
}

/*
 * Read pipe operations.
 */
static struct file_operations read_pipe_fops = {
	.open		= pipe_read_open,
	.release	= pipe_read_release,
	.read		= pipe_read,
	.poll		= pipe_poll,
};

/*
 * Write pipe operations.
 */
static struct file_operations write_pipe_fops = {
	.open		= pipe_write_open,
	.release	= pipe_write_release,
	.write		= pipe_write,
	.poll		= pipe_poll,
};

/*
 * Connecting fifo operations.
 */
struct file_operations connecting_fifo_fops = {
	.open		= pipe_read_open,
	.release	= pipe_read_release,
	.read		= connect_read,
	.poll		= connect_poll,
};

/*
 * Read fifo operations.
 */
struct file_operations read_fifo_fops = {
	.open		= pipe_read_open,
	.release	= pipe_read_release,
	.read		= pipe_read,
	.poll		= fifo_poll,
};

/*
 * Write fifo operations.
 */
struct file_operations write_fifo_fops = {
	.open		= pipe_write_open,
	.release	= pipe_write_release,
	.write		= pipe_write,
	.poll		= fifo_poll,
};

/*
 * Read/write fifo operations.
 */
struct file_operations rdwr_fifo_fops = {
	.open		= pipe_rdwr_open,
	.release	= pipe_rdwr_release,
	.read		= pipe_read,
	.write		= pipe_write,
	.poll		= fifo_poll,
};

/*
 * Get a pipe inode.
 */
static struct inode *get_pipe_inode()
{
	struct inode *inode;

	/* get an empty inode */
	inode = get_empty_inode(NULL);
	if (!inode)
		return NULL;

	/* allocate some memory for data */
	PIPE_BASE(inode) = get_free_page();
	if (!PIPE_BASE(inode)) {
		clear_inode(inode);
		return NULL;
	}

	/* set pipe inode (2 references = reader + writer) */
	inode->i_count = 2;
	inode->i_pipe = 1;
	inode->i_mode = S_IFIFO | S_IRUSR | S_IWUSR;
	inode->i_uid = current_task->fsuid;
	inode->i_gid = current_task->fsgid;
	inode->i_atime = inode->i_ctime = inode->i_mtime = CURRENT_TIME;
	PIPE_WAIT(inode) = NULL;
	PIPE_START(inode) = 0;
	PIPE_LEN(inode) = 0;
	PIPE_READERS(inode) = 1;
	PIPE_WRITERS(inode) = 1;
	PIPE_RD_OPENERS(inode) = 0;
	PIPE_WR_OPENERS(inode) = 0;

	return inode;
}

/*
 * Pipe system call.
 */
static int do_pipe(int pipefd[2], int flags)
{
	struct file *f1 = NULL, *f2 = NULL;
	int fd1 = -1, fd2 = -2, ret;
	struct dentry *dentry;
	struct inode *inode;

	/* get a first file */
	ret = -ENFILE;
	f1 = get_empty_filp();
	if (!f1)
		goto err;

	/* get a second file */
	f2 = get_empty_filp();
	if (!f2)
		goto err_clear_f1;

	/* install first file */
	fd1 = ret = get_unused_fd();
	if (ret < 0)
		goto err_clear_f2;
	current_task->files->filp[fd1] = f1;

	/* install second file */
	fd2 = ret = get_unused_fd();
	if (ret < 0)
		goto err_uninstall_f1;
	current_task->files->filp[fd2] = f2;

	/* get a pipe inode */
	ret = -ENFILE;
	inode = get_pipe_inode();
	if (!inode)
		goto err_uninstall_f2;

	/* allocate a root dentry */
	ret = -ENOMEM;
	dentry = dget(d_alloc_root(inode));
	if (!dentry)
		goto err_release_inode;

	/* set 1st file descriptor as read channel */
	f1->f_dentry = dentry;
	f1->f_pos = 0;
	f1->f_flags |= O_RDONLY | flags;
	f1->f_mode = 1;
	f1->f_op = &read_pipe_fops;
	pipefd[0] = fd1;

	/* set 2nd file descriptor as write channel */
	f2->f_dentry = dentry;
	f1->f_pos = 0;
	f2->f_flags |= O_WRONLY | flags;
	f2->f_mode = 2;
	f2->f_op = &write_pipe_fops;
	pipefd[1] = fd2;

	return 0;
err_release_inode:
	iput(inode);
err_uninstall_f2:
	current_task->files->filp[fd2] = NULL;
err_uninstall_f1:
	current_task->files->filp[fd1] = NULL;
err_clear_f2:
	f2->f_count = 0;
err_clear_f1:
	f1->f_count = 0;
err:
	return ret;
}

/*
 * Pipe system call.
 */
int sys_pipe(int pipefd[2])
{
	return do_pipe(pipefd, 0);
}

/*
 * Pipe2 system call.
 */
int sys_pipe2(int pipefd[2], int flags)
{
	return do_pipe(pipefd, flags);
}
