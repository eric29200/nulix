#ifndef _LIBC_MALLOC_IMPL_H_
#define _LIBC_MALLOC_IMPL_H_

#include <stdio.h>
#include <stdint.h>

#define HEAP_BLOCK_MAGIC	0x464E

/*
 * Heap block.
 */
struct heap_block_t {
	uint32_t		magic;		/* magic number */
	size_t			size;		/* block size */
	uint8_t			free;		/* free block ? */
	struct heap_block_t *	next;		/* next block */
};

#endif