#ifndef _PTY_H_
#define _PTY_H_

#include <fs/dev_fs.h>

#define NR_PTYS			16

/*
 * Pty structure.
 */
struct pty {
	int			p_num;		/* pty id */
	int			p_count;	/* reference count */
	struct devfs_entry *	de;		/* devfs entry */
};

int init_pty();

#endif
