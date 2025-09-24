#include <fs/fs.h>
#include <proc/sched.h>
#include <stderr.h>

/* global file table */
struct file filp_table[NR_FILE];

/*
 * Get an empty file descriptor.
 */
int get_unused_fd()
{
	int fd;

	for (fd = 0; fd < NR_OPEN; fd++)
		if (!current_task->files->filp[fd])
			return fd;

	return -EMFILE;
}

/*
 * Get a file.
 */
struct file *fget(int fd)
{
	struct file *filp;

	if (fd < 0 || fd >= NR_OPEN)
		return NULL;

	filp = current_task->files->filp[fd];
	if (filp)
		filp->f_count++;

	return filp;
}

/*
 * Release a file.
 */
static void __fput(struct file *filp)
{
	/* specific close operation */
	if (filp->f_op && filp->f_op->release)
		filp->f_op->release(filp->f_dentry->d_inode, filp);

	/* release dentry */
	dput(filp->f_dentry);

	/* clear inode */
	memset(filp, 0, sizeof(struct file));
}

/*
 * Release a file.
 */
void fput(struct file *filp)
{
	int count = filp->f_count - 1;
	if (!count)
		__fput(filp);
	filp->f_count = count;
}

/*
 * Get an empty file.
 */
struct file *get_empty_filp()
{
	int i;

	for (i = 0; i < NR_FILE; i++)
		if (!filp_table[i].f_count)
			break;

	if (i >= NR_FILE)
		return NULL;

	filp_table[i].f_count = 1;
	return &filp_table[i];
}

/*
 * Init a private file.
 */
int init_private_file(struct file *filp, struct dentry *dentry, int mode)
{
	/* memzero file */
	memset(filp, 0, sizeof(struct file));

	/* init file */
	filp->f_mode = mode;
	filp->f_count = 1;
	filp->f_dentry = dentry;
	filp->f_op = dentry->d_inode->i_op->fops;

	/* open file */
	if (filp->f_op->open)
		return filp->f_op->open(dentry->d_inode, filp);

	return 0;
}