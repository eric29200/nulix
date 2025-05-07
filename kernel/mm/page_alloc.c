#include <fs/fs.h>
#include <stdio.h>
#include <string.h>

#define NR_NODES		8

/*
 * Memory node.
 */
struct node {
	struct zone *		zone;
	size_t			order;
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

/*
 * Add to free pages.
 */
static void __add_to_free_pages(struct page *pages, int priority, size_t count)
{
	struct zone *zone = &zones[priority];
	uint32_t order, i;

	/* try with biggest order first */
	order = NR_NODES - 1;

	for (i = 0; i < count; ) {
		/* find node */
		while (count - i < (uint32_t) (1 << order))
			order--;

		/* add pages to node */
		list_add_tail(&pages[i].list, &zone->nodes[order].free_pages);

		/* skip pages */
		i += (1 << order);
	}

	/* update number of free pages */
	zones[priority].nr_free_pages += count;
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
	list_del(&page->list);

	/* update number of free pages */
	node->zone->nr_free_pages -= (1 << node->order);

	/* add remaining pages to free list */
	if (order != node->order)
		__add_to_free_pages(page + (1 << order), priority, (1 << node->order) - (1 << order));

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
		__add_to_free_pages(page, page->priority, order);
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
	return __va(page->page_nr * PAGE_SIZE);
}

/*
 * Free a page.
 */
void free_pages(void *address, size_t count)
{
	uint32_t page_idx;

	/* get page index */
	page_idx = MAP_NR((uint32_t) address);

	/* free page */
	if (page_idx && page_idx < nr_pages)
		__free_pages(&page_array[page_idx], count);
}

/*
 * Reclaim pages, when memory is low.
 */
void reclaim_pages()
{
	struct page *page;
	uint32_t i;

	for (i = 0; i < nr_pages; i++) {
		page = &page_array[i];

		/* skip used pages */
		if (page->count > 1)
			continue;

		/* skip shared memory pages */
		if (page->inode && page->inode->i_shm == 1)
			continue;

		/* is it a buffer cached page ? */
		if (page->buffers) {
			try_to_free_buffer(page->buffers);
			continue;
		}

		/* is it a page cached page ? */
		if (page->inode) {
			/* remove from page cache */
			remove_from_page_cache(page);

			/* free page */
			__free_page(page);
		}
	}
}

/*
 * Init page allocation.
 */
void init_page_alloc()
{
	uint32_t page_array_end, addr, order, first_free_kpage, first_free_upage, i;

	/* allocate global pages array */
	page_array = (struct page *) (KPAGE_START + PAGE_ALIGN_UP(KCODE_END));
	memset(page_array, 0, sizeof(struct page) * nr_pages);
	page_array_end = (uint32_t) page_array + sizeof(struct page) * nr_pages;

	/* kernel code pages */
	for (i = 0, addr = 0; i < nr_pages && (uint32_t) __va(addr) < page_array_end; i++, addr += PAGE_SIZE) {
		page_array[i].page_nr = i;
		page_array[i].count = 1;
		page_array[i].priority = GFP_KERNEL;
	}

	/* kernel free pages */
	first_free_kpage = i;
	for (; i < nr_pages && (uint32_t) __va(addr) < KPAGE_END; i++, addr += PAGE_SIZE) {
		page_array[i].page_nr = i;
		page_array[i].count = 0;
		page_array[i].priority = GFP_KERNEL;
	}

	/* user free pages */
	first_free_upage = i;
	for (; i < nr_pages; i++) {
		page_array[i].page_nr = i;
		page_array[i].count = 0;
		page_array[i].priority = GFP_USER;
	}

	/* init memory zones */
	for (i = 0; i < NR_ZONES; i++) {
		for (order = 0; order < NR_NODES; order++) {
			zones[i].nodes[order].zone = &zones[i];
			zones[i].nodes[order].order = order;
			INIT_LIST_HEAD(&zones[i].nodes[order].free_pages);
		}
	}

	/* add kernel and user free pages */
	__add_to_free_pages(&page_array[first_free_kpage], GFP_KERNEL, first_free_upage - first_free_kpage);
	if (nr_pages > first_free_upage)
		__add_to_free_pages(&page_array[first_free_upage], GFP_USER, nr_pages - first_free_upage);
}
