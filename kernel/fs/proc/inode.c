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
	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;
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
