#include <mm/kheap.h>
#include <mm/paging.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>

#define KHEAP_NR_BUCKETS			16
#define KHEAP_MIN_ORDER				16
#define KHEAP_MAGIC				0xAEA0
#define KHEAP_BLOCK_DATA(block)			((uint32_t) (block) + sizeof(struct heap_block))

/*
 * Bucket.
 */
struct bucket {
	size_t			order;
	struct list_head	free_blocks;
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
static struct heap_block *create_heap_block(struct bucket *bucket)
{
	struct heap_block *block;
	size_t npages;
	uint32_t off;
	void *pages;

	/* compute number of pages to allocate */
	npages = PAGE_ALIGN_UP(bucket->order) / PAGE_SIZE;

	/* allocate pages */
	pages = get_free_pages(npages);
	if (!pages)
		return NULL;

	/* create blocks */
	for (off = 0; off < npages * PAGE_SIZE; off += bucket->order) {
		/* create block */
		block = (struct heap_block *) (pages + off);
		block->magic = KHEAP_MAGIC;
		block->order = bucket->order;

		/* add block to free list */
		list_add_tail(&block->list, &bucket->free_blocks);
	}

	/* return first free block */
	block = list_first_entry(&bucket->free_blocks, struct heap_block, list);
	list_del(&block->list);

	return block;
}

/*
 * Allocate memory.
 */
void *kmalloc(size_t size)
{
	struct heap_block *block = NULL;
	struct bucket *bucket;
	size_t real_size;

	/* compute real size */
	real_size = size + sizeof(struct heap_block);

	/* find bucket */
	bucket = find_bucket(real_size);
	if (!bucket) {
		printf("Kheap: can't allocate memory for size %d\n", size);
		return NULL;
	}

	/* try to reuse a free block */
	if (!list_empty(&bucket->free_blocks))
		block = list_first_entry(&bucket->free_blocks, struct heap_block, list);

	/* free block found */
	if (block) {
		list_del(&block->list);
		goto found;
	}

	/* create a new block */
	block = create_heap_block(bucket);
	if (!block) {
		printf("Kheap: kernel heap overflow\n");
		return NULL;
	}

found:
	return (void *) KHEAP_BLOCK_DATA(block);
}

/*
 * Free memory.
 */
void kfree(void *p)
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

	/* add it to free list */
	list_add_tail(&block->list, &bucket->free_blocks);
}

/*
 * Init kernel heap.
 */
void kheap_init()
{
	size_t order, i;

	/* init buckets */
	for (i = 0, order = KHEAP_MIN_ORDER; i < KHEAP_NR_BUCKETS; i++, order *= 2) {
		buckets[i].order = order;
		INIT_LIST_HEAD(&buckets[i].free_blocks);
	}
}
