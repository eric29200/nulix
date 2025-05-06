#include <mm/paging.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>

#define NR_BUCKETS				15
#define MIN_ORDER				16

/*
 * Bucket.
 */
struct bucket {
	size_t				order;
	struct list_head		free_blocks;
};

/*
 * Heap block.
 */
struct heap_block {
	struct bucket *			bucket;
	struct list_head		list;
};

/* kernel heap */
static struct bucket buckets[NR_BUCKETS];

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
		block->bucket = bucket;

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
	size_t real_size, i;

	/* compute real size */
	real_size = size + sizeof(struct heap_block);

	/* find bucket */
	for (i = 0; i < NR_BUCKETS; i++) {
		if (real_size <= buckets[i].order) {
			bucket = &buckets[i];
			break;
		}
	}

	/* no matching bucket */
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
	return block + 1;
}

/*
 * Free memory.
 */
void kfree(void *p)
{
	struct heap_block *block;

	/* dot not free NULL */
	if (!p)
		return;

	/* get block */
	block = (struct heap_block *) ((uint32_t) p - sizeof(struct heap_block));

	/* add it to free list */
	list_add_tail(&block->list, &block->bucket->free_blocks);
}

/*
 * Init kernel heap.
 */
void kheap_init()
{
	size_t order, i;

	/* init buckets */
	for (i = 0, order = MIN_ORDER; i < NR_BUCKETS; i++, order *= 2) {
		buckets[i].order = order;
		INIT_LIST_HEAD(&buckets[i].free_blocks);
	}
}
