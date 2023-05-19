#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "__malloc_impl.h"
#include "../x86/__syscall.h"

/* heap blocks list */
static struct heap_block_t *blocks_list = NULL;

/* brk increments */
static uint32_t brk_current = 0;
static uint32_t brk_free_from_last = 0;

/*
 * Find a free block.
 */
static struct heap_block_t *__find_free_block(struct heap_block_t **last, size_t size)
{
	struct heap_block_t *block;

	for (block = blocks_list, *last = NULL; block != NULL; block = block->next) {
		if (block->free && size <= block->size)
			break;

		*last = block;
	}

	return block;
}

/*
 * Grow heap.
 */
static void *__grow_heap(size_t size)
{
	uint32_t base;

	/* get brk */
	if (!brk_current)
	 	brk_current = __syscall1(SYS_brk, 0);
	base = brk_current;

	/* remaining space */
	if (size <= brk_free_from_last) {
		brk_free_from_last -= size;
		goto out;
	}

	/* request space */
	__syscall1(SYS_brk, brk_current + size);
	brk_free_from_last = __syscall1(SYS_brk, 0) - (brk_current + size);

out:
	brk_current += size;
	return (void *) base;
}

/*
 * Request space.
 */
static struct heap_block_t *__request_space(struct heap_block_t *last, size_t size)
{
	struct heap_block_t *block;

	/* grow heap */
	block = __grow_heap(size + sizeof(struct heap_block_t));

	if (last)
		last->next = block;

	block->magic = HEAP_BLOCK_MAGIC;
	block->size = size;
	block->free = 0;
	block->next = NULL;

	return block;
}

/*
 * Split a block.
 */
static void __split_block(struct heap_block_t *block, size_t size)
{
	struct heap_block_t *splitted;

	/* not enough space to split */
	if (block->size <= size + sizeof(struct heap_block_t))
		return;

	/* 1st = free space */
	splitted = (struct heap_block_t *) ((char *) block + size + sizeof(struct heap_block_t));
	splitted->magic = HEAP_BLOCK_MAGIC;
	splitted->size = block->size - size - sizeof(struct heap_block_t);
	splitted->free = 1;
	splitted->next = block->next;

	/* last = new block */
	block->size = size;
	block->next = splitted;
}

/*
 * Allocate memory.
 */
void *malloc(size_t size)
{
	struct heap_block_t *block, *last;

	if (size <= 0)
		return NULL;

	/* align size to 4 bytes */
	size = ALIGN_UP(size, 4);
	 
	/* try to find a free block */
	block = __find_free_block(&last, size);
	if (block) {
		block->free = 0;
		__split_block(block, size);
		goto out;
	}
	 
	/* else request space */
	block = __request_space(NULL, size);
	if (!blocks_list)
		blocks_list = block;

out:
	return block ? block + 1 : NULL;
}
