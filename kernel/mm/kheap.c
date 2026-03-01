#include <mm/paging.h>
#include <mm/mm.h>
#include <stdio.h>
#include <stderr.h>

#define NR_BUCKETS			15
#define MIN_BLOCK_SIZE			16

/*
 * Heap block.
 */
struct heap_block {
	struct page_descriptor *	page_desc;
	struct list_head		list;
};

/*
 * Page descriptor.
 */
struct page_descriptor {
	struct bucket *			bucket;
	size_t				nr_blocks;
	size_t				nr_free_blocks;
	struct list_head		free_blocks;
	struct list_head		list;
};

/*
 * Bucket.
 */
struct bucket {
	size_t				block_size;
	size_t				pages_order;
	size_t				nr_blocks_per_pages;
	struct list_head		free_pages;
};

/* kernel heap */
static struct bucket buckets[NR_BUCKETS];

/*
 * Create new heap blocks.
 */
static struct page_descriptor *create_heap_blocks(struct bucket *bucket)
{
	struct page_descriptor *page_desc;
	struct heap_block *block;
	uint32_t off, i;
	void *pages;

	/* allocate pages */
	pages = get_free_pages(bucket->pages_order);
	if (!pages)
		return NULL;

	/* create page descriptor */
	page_desc = (struct page_descriptor *) pages;
	page_desc->bucket = bucket;
	INIT_LIST_HEAD(&page_desc->free_blocks);
	page_desc->nr_blocks = bucket->nr_blocks_per_pages;
	page_desc->nr_free_blocks = bucket->nr_blocks_per_pages;

	/* create heap blocks */
	off = sizeof(struct page_descriptor);
	for (i = 0; i < page_desc->nr_blocks; i++) {
		/* create block */
		block = (struct heap_block *) (pages + off);
		block->page_desc = page_desc;

		/* add block to free list */
		list_add_tail(&block->list, &page_desc->free_blocks);

		/* go to next block */
		off += bucket->block_size;
	}

	/* add page to free list */
	list_add_tail(&page_desc->list, &bucket->free_pages);

	return page_desc;
}

/*
 * Allocate memory.
 */
void *kmalloc(size_t size)
{
	struct page_descriptor *page_desc;
	struct heap_block *block;
	struct bucket *bucket;
	size_t real_size, i;

	/* compute real size */
	real_size = size + sizeof(struct heap_block);

	/* find bucket */
	for (i = 0; i < NR_BUCKETS; i++) {
		if (real_size <= buckets[i].block_size) {
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
	if (!list_empty(&bucket->free_pages))
		page_desc = list_first_entry(&bucket->free_pages, struct page_descriptor, list);
	else
		page_desc = create_heap_blocks(bucket);

	/* can't get memory */
	if (!page_desc) {
		printf("Kheap: kernel heap overflow\n");
		return NULL;
	}

	/* pop first free block */
	block = list_first_entry(&page_desc->free_blocks, struct heap_block, list);
	list_del(&block->list);
	page_desc->nr_free_blocks--;

	/* no more free block in page : remove page from free list */
	if (!page_desc->nr_free_blocks)
		list_del(&page_desc->list);

	return block + 1;
}

/*
 * Free memory.
 */
void kfree(void *p)
{
	struct page_descriptor *page_desc;
	struct heap_block *block;
	struct bucket *bucket;

	/* dot not free NULL */
	if (!p)
		return;

	/* get block */
	block = (struct heap_block *) ((uint32_t) p - sizeof(struct heap_block));
	page_desc = block->page_desc;
	bucket = page_desc->bucket;

	/* add block to free list */
	list_add_tail(&block->list, &page_desc->free_blocks);
	page_desc->nr_free_blocks++;

	/* add page to free list if needed */
	if (page_desc->nr_free_blocks == 1) {
		list_del(&page_desc->list);
		list_add_tail(&page_desc->list, &bucket->free_pages);
	}

	/* no more used blocks : free pages */
	if (page_desc->nr_free_blocks == page_desc->nr_blocks) {
		list_del(&page_desc->list);
		free_pages(page_desc, bucket->pages_order);
	}
}

/*
 * Init kernel heap.
 */
void kheap_init()
{
	size_t block_size, npages, i;

	/* init buckets */
	for (i = 0, block_size = MIN_BLOCK_SIZE; i < NR_BUCKETS; i++, block_size *= 2) {
		/* init bucket */
		buckets[i].block_size = block_size;
		buckets[i].pages_order = 0;
		INIT_LIST_HEAD(&buckets[i].free_pages);

		/* compute number of pages to allocate to get at least one block */
		npages = PAGE_ALIGN_UP(block_size + sizeof(struct page_descriptor)) / PAGE_SIZE;
		while (npages > (uint32_t) (1 << buckets[i].pages_order))
			buckets[i].pages_order++;

		/* compute number of blocks per pages */
		buckets[i].nr_blocks_per_pages = (npages * PAGE_SIZE - sizeof(struct page_descriptor)) / block_size;
	}
}
