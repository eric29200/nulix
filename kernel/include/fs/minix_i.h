#ifndef _MINIX_I_H_
#define _MINIX_I_H_

#include <stddef.h>

/*
 * Minix in memory inode.
 */
struct minix_inode_info {
	union {
		uint16_t		i1_data[16];
		uint32_t		i2_data[16];
	} u;
};

#endif
