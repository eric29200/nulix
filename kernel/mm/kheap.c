#include <mm/kheap.h>
#include <mm/paging.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>

#define HEAP_BLOCK_DATA(block)			((uint32_t) (block) + sizeof(struct heap_block))
#define HEAP_BLOCK_ALIGNED(block)		(PAGE_ALIGNED(HEAP_BLOCK_DATA(block)))

/* kernel heap */
struct kheap *kheap = NULL;

/*
 * Create a heap.
 */
int kheap_init(uint32_t start_address, size_t size)
{
	/* check heap size */
	if (size <= sizeof(struct heap_block))
		return -EINVAL;

	/* allocate new heap */
	kheap = kmalloc(sizeof(struct kheap));
	if (!kheap)
		return -ENOMEM;

	/* set start/end addresses */
	kheap->first_block = (struct heap_block *) start_address;
	kheap->start_address = start_address;
	kheap->end_address = start_address + size;
	kheap->size = size;

	/* create first block */
	kheap->first_block->magic = HEAP_MAGIC;
	kheap->first_block->size = size - sizeof(struct heap_block);
	kheap->first_block->free = 1;
	kheap->first_block->prev = NULL;
	kheap->first_block->next = NULL;

	return 0;
}

/*
 * Find a free block.
 */
static struct heap_block *kheap_find_free_block(uint8_t page_aligned, size_t size)
{
	struct heap_block *block;
	uint32_t page_offset;

	for (block = kheap->first_block; block != NULL; block = block->next) {
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
void *kheap_alloc(size_t size, uint8_t page_aligned)
{
	struct heap_block *block, *new_block, *prev, *next;
	uint32_t page_offset, block_size;

	/* find free block */
	block = kheap_find_free_block(page_aligned, size);
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
			kheap->first_block = block;
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
void kheap_free(void *p)
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
void kheap_dump()
{
	struct heap_block *block;

	for (block = kheap->first_block; block != NULL; block = block->next)
		printf("%x\t%d\t%d\n", (uint32_t) block, block->size, block->free);
}
