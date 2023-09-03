#ifndef _ISO_I_H_
#define _ISO_I_H_

#include <stddef.h>

/*
 * Isofs in memory inode.
 */
struct isofs_inode_info {
	uint32_t	i_first_extent;
	uint32_t	i_backlink;
};

#endif
