#ifndef _PROC_I_H_
#define _PROC_I_H_

struct dentry;
struct task;

/*
 * Procfs operation.
 */
union proc_op {
	int				(*proc_read)(struct task *, char *);
	int				(*proc_get_link)(struct task *, struct dentry **);
};

/*
 * Proc in memory inode.
 */
struct proc_inode_info {
	pid_t			pid;
	int			fd;
	union proc_op		op;
};

#endif
