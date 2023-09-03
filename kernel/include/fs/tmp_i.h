#ifndef _TMPFS_I_H_
#define _TMPFS_I_H_

#include <lib/list.h>
#include <stddef.h>

/*
 * Tmpfs in memory inode.
 */
struct tmpfs_inode_info {
	struct list_head	i_pages;
	int			i_shmid;
};

#endif
