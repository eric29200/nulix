#include <mm/heap.h>
#include <mm/paging.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>

#define HEAP_BLOCK_DATA(block)			((uint32_t) (block) + sizeof(struct heap_block))
#define HEAP_BLOCK_ALIGNED(block)		(PAGE_ALIGNED(HEAP_BLOCK_DATA(block)))

/*
 * Create a heap.
 */
struct heap *heap_create(uint32_t start_address, size_t size)
{
	struct heap *heap;

	/* check heap size */
	if (size <= sizeof(struct heap_block))
		return NULL;

	/* allocate new heap */
	heap = kmalloc(sizeof(struct heap));
	if (!heap)
		return NULL;

	/* set start/end addresses */
	heap->first_block = (struct heap_block *) start_address;
	heap->start_address = start_address;
	heap->end_address = start_address + size;
	heap->size = size;

	/* create first block */
	heap->first_block->magic = HEAP_MAGIC;
	heap->first_block->size = size - sizeof(struct heap_block);
	heap->first_block->free = 1;
	heap->first_block->prev = NULL;
	heap->first_block->next = NULL;

	return heap;
}

/*
 * Find a free block.
 */
static struct heap_block *heap_find_free_block(struct heap *heap, uint8_t page_aligned, size_t size)
{
	struct heap_block *block;
	uint32_t page_offset;

	for (block = heap->first_block; block != NULL; block = block->next) {
		/* skip busy blocks */
		if (!block->free)
			continue;

		/* compute page offset */
		page_offset = 0;
		if (page_aligned && !HEAP_BLOCK_ALIGNED(block))
			page_offset = PAGE_SIZE - HEAP_BLOCK_DATA(block) % PAGE_SIZE;

		/* check size */
		if (block->size >= size + page_offset)
			return block;
	}

	return NULL;
}

/*
 * Allocate memory on the heap.
 */
void *heap_alloc(struct heap *heap, size_t size, uint8_t page_aligned)
{
	struct heap_block *block, *new_block, *prev, *next;
	uint32_t page_offset, block_size;

	/* find free block */
	block = heap_find_free_block(heap, page_aligned, size);
	if (!block)
		return NULL;

	/* if page alignement is asked, move block */
	if (page_aligned && !HEAP_BLOCK_ALIGNED(block)) {
		/* compute page offset */
		page_offset = PAGE_SIZE - HEAP_BLOCK_DATA(block) % PAGE_SIZE;

		/* move block */
		block_size = block->size;
		prev = block->prev;
		next = block->next;
		block = (void *) block + page_offset;
		block->magic = HEAP_MAGIC;
		block->size = block_size - page_offset;
		block->prev = prev;
		block->next = next;

		/* update previous block size */
		if (block->prev) {
			block->prev->size += page_offset;
			block->prev->next = block;
		} else {
			heap->first_block = block;
		}

		/* update next block */
		if (block->next)
			block->next->prev = block;
	}

	/* create new free block with remaining size */
	if (block->size - size > sizeof(struct heap_block)) {
		new_block = (struct heap_block *) (HEAP_BLOCK_DATA(block) + size);
		new_block->magic = HEAP_MAGIC;
		new_block->size = block->size - size - sizeof(struct heap_block);
		new_block->free = 1;
		new_block->prev = block;
		new_block->next = block->next;

		/* update this block */
		block->size = size;
		block->next = new_block;
	}

	/* mark this block */
	block->free = 0;

	return (void *) HEAP_BLOCK_DATA(block);
}

/*
 * Free memory on the heap.
 */
void heap_free(void *p)
{
	struct heap_block *block;

	/* do not free NULL */
	if (!p)
		return;

	/* mark block as free */
	block = (struct heap_block *) ((uint32_t) p - sizeof(struct heap_block));

	/* check if it's a heap block */
	if (block->magic != HEAP_MAGIC)
		return;

	/* mark block as free */
	block->free = 1;

	/* merge with right block */
	if (block->next && block->next->free) {
		block->size = HEAP_BLOCK_DATA(block->next) + block->next->size - HEAP_BLOCK_DATA(block);
		block->next = block->next->next;

		if (block->next)
			block->next->prev = block;
	}
}

/*
 * Dump the heap on the screen.
 */
void heap_dump(struct heap *heap)
{
	struct heap_block *block;

	for (block = heap->first_block; block != NULL; block = block->next)
		printf("%x\t%d\t%d\n", (uint32_t) block, block->size, block->free);
}
