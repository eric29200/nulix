#include <mm/kheap.h>
#include <mm/paging.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>

#define KHEAP_NR_BUCKETS			16
#define KHEAP_MIN_ORDER				16
#define KHEAP_MAGIC				0xAEA0
#define KHEAP_BLOCK_DATA(block)			((uint32_t) (block) + sizeof(struct heap_block))
#define KHEAP_BLOCK_ALIGNED(block)		(PAGE_ALIGNED(KHEAP_BLOCK_DATA(block)))

/*
 * Bucket.
 */
struct bucket {
	size_t			order;
	struct list_head	used_blocks;
	struct list_head	free_blocks;	
	struct list_head	free_aligned_blocks;	
};

/*
 * Heap block.
 */
struct heap_block {
	uint16_t		magic;
	size_t			order;
	struct list_head	list;
};

/* kernel heap */
uint32_t kheap_pos = 0;
static struct bucket buckets[KHEAP_NR_BUCKETS];

/*
 * Find a bucket.
 */
static struct bucket *find_bucket(size_t size)
{
	size_t i;

	for (i = 0; i < KHEAP_NR_BUCKETS; i++)
		if (size <= buckets[i].order)
			return &buckets[i];

	return NULL;
}

/*
 * Create a new heap block.
 */
static struct heap_block *create_heap_block(struct bucket *bucket, int page_aligned)
{
	struct heap_block *block;
	uint32_t page_offset;

	/* create new block at the end of the heap */
	block = (struct heap_block *) kheap_pos;

	/* page align block if needed */
	if (page_aligned && !KHEAP_BLOCK_ALIGNED(block)) {
		page_offset = PAGE_SIZE - KHEAP_BLOCK_DATA(block) % PAGE_SIZE;
		block = (void *) block + page_offset;
	}

	/* check heap overflow */
	if (KHEAP_BLOCK_DATA(block) + bucket->order > KHEAP_START + KHEAP_SIZE)
		return NULL;

	/* set new block */
	block->magic = KHEAP_MAGIC;
	block->order = bucket->order;

	/* update kheap position */
	kheap_pos = KHEAP_BLOCK_DATA(block) + block->order;

	return block;
}

/*
 * Allocate memory.
 */
void *kheap_alloc(size_t size, int page_aligned)
{
	struct heap_block *block = NULL;
	struct bucket *bucket;
	
	/* find bucket */
	bucket = find_bucket(size);
	if (!bucket) {
		printf("Kheap: can't allocate memory for size %d\n", size);
		return NULL;
	}

	/* try to reuse a free block */
	if (page_aligned && !list_empty(&bucket->free_aligned_blocks))
		block = list_first_entry(&bucket->free_aligned_blocks, struct heap_block, list);
	else if (!page_aligned && !list_empty(&bucket->free_blocks))
		block = list_first_entry(&bucket->free_blocks, struct heap_block, list);

	/* free block found */
	if (block) {
		list_del(&block->list);
		goto found;
	}

	/* create a new block */
	block = create_heap_block(bucket, page_aligned);
	if (!block) {
		printf("Kheap: kernel heap overflow\n");
		return NULL;
	}

found:
	list_add(&block->list, &bucket->used_blocks);
	return (void *) KHEAP_BLOCK_DATA(block);
}

/*
 * Free memory.
 */
void kheap_free(void *p)
{
	struct heap_block *block;
	struct bucket *bucket;

	/* dot not free NULL */
	if (!p)
		return;

	/* get block */
	block = (struct heap_block *) ((uint32_t) p - sizeof(struct heap_block));
	if (block->magic != KHEAP_MAGIC)
		return;

	/* get bucket */
	bucket = find_bucket(block->order);
	if (!bucket)
		return;

	/* remove if from used list */
	list_del(&block->list);

	/* add to free list */
	if (KHEAP_BLOCK_ALIGNED(block))
		list_add_tail(&block->list, &bucket->free_aligned_blocks);
	else
		list_add_tail(&block->list, &bucket->free_blocks);
}

/*
 * Init kernel heap.
 */
void kheap_init()
{
	size_t order, i;

	/* set kheap */
	kheap_pos = KHEAP_START;

	/* init buckets */
	for (i = 0, order = KHEAP_MIN_ORDER; i < KHEAP_NR_BUCKETS; i++, order *= 2) {
		buckets[i].order = order;
		INIT_LIST_HEAD(&buckets[i].used_blocks);
		INIT_LIST_HEAD(&buckets[i].free_blocks);
		INIT_LIST_HEAD(&buckets[i].free_aligned_blocks);
	}
}