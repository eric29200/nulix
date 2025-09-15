#ifndef _PTY_H_
#define _PTY_H_

#define NR_PTYS			16

#include <fs/devpts_fs.h>

/*
 * Pty structure.
 */
struct pty {
	int			p_num;		/* pty id */
	int			p_count;	/* reference count */
	struct devpts_entry *	de;		/* devpts entry */
};

int init_pty(struct file_operations *fops);

#endif
