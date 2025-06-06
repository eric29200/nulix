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
static int pipe_read(struct file *filp, char *buf, int count)
{
	struct inode *inode = filp->f_inode;
	int chars, size, read = 0;
	char *pipebuf;

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

	if (read)
		return read;

	if (PIPE_WRITERS(inode))
		return -EAGAIN;

	return 0;
}

/*
 * Write to a pipe.
 */
static int pipe_write(struct file *filp, const char *buf, int count)
{
	struct inode *inode = filp->f_inode;
	int chars, free, written = 0;
	char *pipebuf;

	/* no readers */
	if (!PIPE_READERS(inode)) {
		task_signal(current_task->pid, SIGPIPE);
		return -EPIPE;
	}

	while (count > 0) {
		/* no free space */
		while (!PIPE_EMPTY(inode)) {
			/* no readers */
			if (!PIPE_READERS(inode)) {
				task_signal(current_task->pid, SIGPIPE);
				return written ? written : -EPIPE;
			}

			/* process interruption */
			if (signal_pending(current_task))
				return written ? written : -ERESTARTSYS;

			/* non blocking */
			if (filp->f_flags & O_NONBLOCK)
				return written ? written : -EAGAIN;

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

	return written;
}

/*
 * Close a read pipe.
 */
static int pipe_read_close(struct file *filp)
{
	struct inode *inode = filp->f_inode;

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
static int pipe_write_close(struct file *filp)
{
	struct inode *inode = filp->f_inode;

	PIPE_WRITERS(inode)--;
	if (!PIPE_READERS(inode) && !PIPE_WRITERS(inode))
		free_page(PIPE_BASE(inode));
	else
		wake_up(&PIPE_WAIT(inode));

	return 0;
}

/*
 * Poll a pipe.
 */
static int pipe_poll(struct file *filp, struct select_table *wait)
{
	int mask;

	/* reader or writer */
	if (PIPE_EMPTY(filp->f_inode))
		mask = POLLOUT;
	else
		mask = POLLIN;

	/* add wait queue to select table */
	select_wait(&PIPE_WAIT(filp->f_inode), wait);

	return mask;
}

/*
 * Read pipe operations.
 */
static struct file_operations read_pipe_fops = {
	.read		= pipe_read,
	.close		= pipe_read_close,
	.poll		= pipe_poll,
};

/*
 * Write pipe operations.
 */
static struct file_operations write_pipe_fops = {
	.write		= pipe_write,
	.close		= pipe_write_close,
	.poll		= pipe_poll,
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

	return inode;
}

/*
 * Pipe system call.
 */
static int do_pipe(int pipefd[2], int flags)
{
	struct file *filps[2];
	struct inode *inode;
	int fd[2];
	int i, j;

	/* find 2 file descriptors in global table */
	for (i = 0, j = 0; i < NR_FILE && j < 2; i++)
		if (!filp_table[i].f_count)
			filps[j++] = &filp_table[i];

	/* not enough available slots */
	if (j < 2)
		return -ENOSPC;

	/* update reference counts */
	filps[0]->f_count++;
	filps[1]->f_count++;

	/* find 2 file descriptors in current task */
	for (i = 0, j = 0; i < NR_OPEN && j < 2; i++) {
		if (!current_task->files->filp[i]) {
			fd[j] = i;
			current_task->files->filp[i] = filps[j++];
		}
	}

	/* not enough available slots */
	if (j < 2) {
		if (j == 1)
			current_task->files->filp[fd[0]] = NULL;

		filps[0]->f_count = 0;
		filps[1]->f_count = 0;
		return -ENOSPC;
	}

	/* get a pipe inode */
	inode = get_pipe_inode();
	if (!inode) {
		current_task->files->filp[fd[0]] = NULL;
		current_task->files->filp[fd[1]] = NULL;
		filps[0]->f_count = 0;
		filps[1]->f_count = 0;
		return -ENOSPC;
	}

	/* set 1st file descriptor as read channel */
	filps[0]->f_inode = inode;
	filps[0]->f_pos = 0;
	filps[0]->f_flags |= O_WRONLY | flags;
	filps[0]->f_mode = 1;
	filps[0]->f_op = &read_pipe_fops;
	pipefd[0] = fd[0];

	/* set 2nd file descriptor as write channel */
	filps[1]->f_inode = inode;
	filps[1]->f_pos = 0;
	filps[1]->f_flags |= O_WRONLY | flags;
	filps[1]->f_mode = 2;
	filps[1]->f_op = &write_pipe_fops;
	pipefd[1] = fd[1];

	return 0;
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
