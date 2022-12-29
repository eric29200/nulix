#ifndef _DEVFS_I_H_
#define _DEVFS_I_H_

#include <lib/list.h>
#include <stddef.h>

/*
 * Devfs in memory inode.
 */
struct devfs_inode_info_t {
	struct list_head_t	i_pages;
};

#endif
