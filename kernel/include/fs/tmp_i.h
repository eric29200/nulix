#ifndef _TMP_I_H_
#define _TMP_I_H_

#include <lib/list.h>
#include <stddef.h>

/*
 * Tmpfs in memory inode.
 */
struct tmp_inode_info_t {
	struct list_head_t	i_pages;
};

#endif