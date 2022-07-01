#ifndef _MINIX_I_H_
#define _MINIX_I_H_

#include <stddef.h>

/*
 * Minix in memory inode.
 */
struct minix_inode_info_t {
	uint32_t		i_zone[10];
};

#endif
