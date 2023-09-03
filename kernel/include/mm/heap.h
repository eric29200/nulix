#ifndef _MM_HEAP_H_
#define _MM_HEAP_H_

#include <mm/paging.h>
#include <stddef.h>

#define HEAP_MAGIC		0xAEA0

/*
 * Heap block header.
 */
struct heap_block {
	uint16_t		magic;
	uint32_t		size;
	uint8_t			free;
	struct heap_block *	prev;
	struct heap_block *	next;
};

/*
 * Heap structure.
 */
struct heap {
	struct heap_block *	first_block;
	struct heap_block *	last_block;
	uint32_t		start_address;
	uint32_t		end_address;
	size_t			size;
};

struct heap *heap_create(uint32_t start_address, size_t size);
void *heap_alloc(struct heap *heap, size_t size, uint8_t page_aligned);
void heap_free(struct heap *heap, void *p);
void heap_dump(struct heap *heap);

#endif
