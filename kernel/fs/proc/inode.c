#include <fs/proc_fs.h>
#include <proc/sched.h>
#include <stderr.h>
#include <stdio.h>
#include <fcntl.h>

/*
 * Get a procfs inode.
 */
struct inode *proc_get_inode(struct super_block *sb, ino_t ino, struct proc_dir_entry *de)
{
	struct list_head *pos;
	struct inode *inode;
	struct task *task;

	/* get inode */
	inode = iget(sb, ino);
	if (!inode)
		return NULL;

	/* set proc dir entry */
	inode->u.generic_i = (void *) de;

	/* set inode */
	if (de) {
		if (de->mode) {
			inode->i_mode = de->mode;
			inode->i_uid = de->uid;
			inode->i_gid = de->gid;
		}
		if (de->size)
			inode->i_size = de->size;
		if (de->ops)
			inode->i_op = de->ops;
		if (de->nlink)
			inode->i_nlinks = de->nlink;
	}

	/* fixup the root inode's nlink value */
	if (inode->i_ino == PROC_ROOT_INO) {
		list_for_each(pos, &tasks_list) {
			task = list_entry(pos, struct task, list);
			if (task && task->pid)
				inode->i_nlinks++;
		}
	}

	return inode;
}

/*
 * Read an inode.
 */
int proc_read_inode(struct inode * inode)
{
	struct file *filp;
	struct task *task;
	ino_t ino;
	pid_t pid;

	/* set inode */
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
	inode->i_blocks = 0;
	inode->i_op = NULL;
	inode->i_mode = 0;
	inode->i_uid = 0;
	inode->i_gid = 0;
	inode->i_nlinks = 1;
	inode->i_size = 0;
	ino = inode->i_ino;

	/* get pid */
	pid = ino >> 16;
	if (!pid)
		return 0;

	/* get task */
	task = find_task(pid);
	if (!task)
		return 0;

	/* pid folder : change uid/gid */
	ino &= 0x0000FFFF;
	if (ino == PROC_PID_INO) {
		inode->i_uid = task->euid;
		inode->i_gid = task->egid;
	}

	/* pid/fd folder */
	if (ino & PROC_PID_FD_DIR) {
		/* get file */
		ino &= 0x7FFF;
		if (ino >= NR_OPEN || !task->files->filp[ino])
			return 0;
		filp = task->files->filp[ino];

		/* set inode */
		inode->i_op = &proc_link_iops;
		inode->i_size = 64;
		inode->i_mode = S_IFLNK;
		if (filp->f_mode & 1)
			inode->i_mode |= S_IRUSR | S_IXUSR;
		if (filp->f_mode & 2)
			inode->i_mode |= S_IWUSR | S_IXUSR;
	}

	return 0;
}

/*
 * Write an inode.
 */
int proc_write_inode(struct inode *inode)
{
	UNUSED(inode);
	return 0;
}

/*
 * Release an inode.
 */
int proc_put_inode(struct inode *inode)
{
	if (!inode->i_count)
		clear_inode(inode);

	return 0;
}
