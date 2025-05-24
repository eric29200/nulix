#include <fs/fs.h>
#include <mm/highmem.h>
#include <stdio.h>
#include <string.h>

#define NR_NODES		8

/*
 * Memory node.
 */
struct node {
	struct zone *		zone;
	size_t			order;
	size_t			order_nr_pages;
	struct list_head	free_pages;
};

/*
 * Memory zone.
 */
struct zone {
	size_t			nr_free_pages;
	struct node		nodes[NR_NODES];
};

/* Memory zones */
static struct zone zones[NR_ZONES];
uint32_t totalram_pages = 0;

/*
 * Get number of free pages.
 */
uint32_t nr_free_pages()
{
	uint32_t ret = 0;
	int i;

	for (i = 0; i < NR_ZONES; i++)
		ret += zones[i].nr_free_pages;

	return ret;
}

/*
 * Add to free pages.
 */
static void __add_to_free_pages(struct page *pages, int priority, size_t count)
{
	struct zone *zone = &zones[priority];
	struct node *node;
	uint32_t i;

	/* try with max order first */
	node = &zone->nodes[NR_NODES - 1];

	for (i = 0; i < count; ) {
		/* find node */
		while (count - i < node->order_nr_pages)
			node--;

		/* add pages to node */
		list_add_tail(&pages[i].list, &node->free_pages);
		pages[i].private = node;

		/* skip pages */
		i += node->order_nr_pages;
	}

	/* update number of free pages */
	zones[priority].nr_free_pages += count;
}

/*
 * Delete from free pages.
 */
static void __delete_from_free_pages(struct page *pages)
{
	struct node *node = pages->private;

	/* update number of free pages */
	node->zone->nr_free_pages -= node->order_nr_pages;

	/* remove pages from free list */
	pages->private = NULL;
	list_del(&pages->list);
}

/*
 * Find free node for "order" pages allocation.
 */
static struct node *__find_free_node(int priority, uint32_t order)
{
	struct zone *zone;

	/* no free page in the zone */
	zone = &zones[priority];
	if (!zone->nr_free_pages)
		return NULL;

	/* check order */
	if (order >= NR_NODES) {
		printf("Page allocation failed: can't allocate pages (priority %d, order %d)\n", priority, order);
		return NULL;
	}

	/* find smallest node with free pages */
	for (; order < NR_NODES; order++)
		if (!list_empty(&zone->nodes[order].free_pages))
			return &zone->nodes[order];

	return NULL;
}

/*
 * Get free pages.
 */
struct page *__get_free_pages(int priority, uint32_t order)
{
	size_t npages = 1 << order;
	struct node *node;
	struct page *page;

	/* find free node */
	node = __find_free_node(priority, order);
	if (node)
		goto found;

	/* try to use kernel pages */
	if (priority != GFP_KERNEL) {
		node = __find_free_node(GFP_KERNEL, order);
		if (node)
			goto found;
	}

	/* reclaim pages */
	reclaim_pages();

	/* retry one last time */
	node = __find_free_node(GFP_KERNEL, order);
	if (node)
		goto found;

	return NULL;
found:
	/* get first free page */
	page = list_first_entry(&node->free_pages, struct page, list);
	page->inode = NULL;
	page->offset = 0;
	page->buffers = NULL;
	page->count = 1;
	__delete_from_free_pages(page);

	/* add remaining pages to free list */
	if (order != node->order)
		__add_to_free_pages(page + npages, priority, node->order_nr_pages - npages);

	return page;
}

/*
 * Free pages.
 */
void __free_pages(struct page *page, uint32_t order)
{
	if (!page)
		return;

	page->count--;
	if (!page->count) {
		page->inode = NULL;
		__add_to_free_pages(page, page->priority, 1 << order);
	}
}

/*
 * Get a free page.
 */
void *get_free_pages(uint32_t order)
{
	struct page *page;

	/* get a free page */
	page = __get_free_pages(GFP_KERNEL, order);
	if (!page)
		return NULL;

	/* make virtual address */
	return page_address(page);
}

/*
 * Free a page.
 */
void free_pages(void *address, uint32_t order)
{
	uint32_t page_idx;

	/* get page index */
	page_idx = MAP_NR((uint32_t) address);

	/* free page */
	if (page_idx && page_idx < nr_pages)
		__free_pages(&page_array[page_idx], order);
}

/*
 * Merge free contiguous pages.
 */
static void merge_free_pages()
{
	struct page *first_pages, *pages, *next_pages;
	struct node *first_node, *node, *next_node;
	uint32_t nr_free, page_nr, i;

	/* for each page */
	for (i = 0; i < nr_pages; ) {
		/* get page group */
		first_pages = pages = &page_array[i];
		first_node = node = pages->private;

		/* page group not free */
		if (!node) {
			i++;
			continue;
		}

		/* find contiguous free pages */
		nr_free = 0;
		for (page_nr = i; page_nr + node->order_nr_pages < nr_pages; ) {
			/* get next page group */
			next_pages = &page_array[page_nr + node->order_nr_pages];
			next_node = next_pages->private;

			/* not free or different priority */
			if (!next_node || pages->priority != next_pages->priority)
				break;

			/* delete from free pages */
			__delete_from_free_pages(next_pages);

			/* update number of free pages */
			nr_free += next_node->order_nr_pages;

			/* go to next group */
			page_nr += node->order_nr_pages;
			pages = &page_array[page_nr];
			node = next_node;
		}

		/* free contiguous pages found */
		if (nr_free) {
			/* remove first pages first */
			__delete_from_free_pages(first_pages);
			nr_free += first_node->order_nr_pages;

			/* add all pages to free list */
			__add_to_free_pages(first_pages, first_pages->priority, nr_free);
		}

		/* go to next group */
		if (nr_free)
			i += nr_free;
		else
			i++;
	}
}

/*
 * Reclaim pages, when memory is low.
 */
void reclaim_pages()
{
	struct buffer_head *bh;
	struct page *page;
	uint32_t i;

	for (i = 0; i < nr_pages; i++) {
		page = &page_array[i];
		bh = page->buffers;

		/* skip used pages */
		if (page->count > 1)
			continue;

		/* skip shared memory pages */
		if (page->inode && page->inode->i_shm == 1)
			continue;

		/* is it a page cached page ? */
		if (page->inode) {
			/* remove from page cache */
			remove_from_page_cache(page);

			/* free page */
			__free_page(page);

			continue;
		}

		/* is it a buffer cached page ? */
		if (bh)
			try_to_free_buffer(bh);
	}

	/* merge free pages */
	merge_free_pages();
}

/*
 * Init a zone.
 */
static void __init_zone(int priority)
{
	uint32_t order, start = 0, end = 0, addr, i;
	struct zone *zone = &zones[priority];
	struct page *first_free_page = NULL;

	/* clear number of free pages */
	zone->nr_free_pages = 0;

	/* init nodes */
	for (order = 0; order < NR_NODES; order++) {
		zone->nodes[order].zone = &zones[priority];
		zone->nodes[order].order = order;
		zone->nodes[order].order_nr_pages = (1 << order);
		INIT_LIST_HEAD(&zone->nodes[order].free_pages);
	}

	/* kernel pages from 0 to PAGE_OFFSET */
	if (priority == GFP_KERNEL) {
		start = 0;
		end = __pa(KPAGE_END);
	} else if (priority == GFP_HIGHUSER) {
		start = __pa(KPAGE_END);
		end = nr_pages * PAGE_SIZE;
	}

	/* for each page */
	for (addr = start, i = start / PAGE_SIZE; i < nr_pages && addr < end; i++, addr += PAGE_SIZE) {
		/* set priority */
		page_array[i].priority = priority;

		/* set virtual address for kernel pages */
		if (priority == GFP_KERNEL)
			page_array[i].virtual = (void *) __va(addr);

		/* page not available */
		if (!bios_map_address_available(addr)) {
			page_array[i].count = 1;

			if (first_free_page) {
				__add_to_free_pages(first_free_page, priority, &page_array[i] - first_free_page);
				totalram_pages += &page_array[i] - first_free_page;
			}

			first_free_page = NULL;
			continue;
		}

		/* set first free page */
		if (!first_free_page)
			first_free_page = &page_array[i];
	}

	/* add last free pages */
	if (first_free_page) {
		__add_to_free_pages(first_free_page, priority, &page_array[i] - first_free_page);
		totalram_pages += &page_array[i] - first_free_page;
	}
}

/*
 * Init page allocation.
 */
int init_page_alloc(uint32_t kernel_start, uint32_t kernel_end)
{
	int ret;

	/* allocate global pages array */
	page_array = (struct page *) __va(kernel_end);
	kernel_end += sizeof(struct page) * nr_pages;
	memset(page_array, 0, sizeof(struct page) * nr_pages);

	/* reserve memory for kernel code */
	ret = bios_map_add_entry(kernel_start, kernel_end, MULTIBOOT_MEMORY_RESERVED);
	if (ret)
		return ret;

	/* init zones */
	__init_zone(GFP_KERNEL);
	__init_zone(GFP_HIGHUSER);

	/* set first high mem page */
	if (nr_pages > __pa(KPAGE_END) / PAGE_SIZE)
		highmem_start_page = &page_array[__pa(KPAGE_END) / PAGE_SIZE];
	else
		highmem_start_page = &page_array[nr_pages];

	return 0;
}
