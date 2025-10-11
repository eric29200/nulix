#ifndef _PROC_I_H_
#define _PROC_I_H_

struct dentry;

/*
 * Proc in memory inode.
 */
struct proc_inode_info {
	struct task *		task;
	struct file *		filp;
	int			(*proc_read)(struct task *, char *);
	int			(*proc_get_link)(struct task *, struct dentry **);
};

#endif