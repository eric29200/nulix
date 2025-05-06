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
static void __add_to_free_pages(struct page *pages, size_t count, int priority)
{
	struct node *node;
	size_t i;

	/* try with biggest node first */
	node = &zones[priority].nodes[NR_NODES - 1];

	for (i = 0; i < count; ) {
		/* find node */
		while (count - i < node->order)
			node--;

		/* add pages to node */
		list_add_tail(&pages[i].list, &node->free_pages);

		/* skip pages */
		i += node->order;
	}

	/* update number of free pages */
	zones[priority].nr_free_pages += count;
}

/*
 * Find free node for "count" pages allocation.
 */
static struct node *__find_free_node(size_t count, int priority)
{
	struct node *node;
	int i;

	/* no free page in the zone */
	if (!zones[priority].nr_free_pages)
		return NULL;

	/* find zone */
	for (i = 0; i < NR_NODES; i++) {
		if (count <= zones[priority].nodes[i].order) {
			node = &zones[priority].nodes[i];
			break;
		}
	}

	/* no matching node */
	if (!node) {
		printf("Page allocation failed: can't allocate %d pages\n", count);
		return NULL;
	}

	/* free pages available in node */
	if (!list_empty(&node->free_pages))
		return node;

	/* try bigger zones */
	for (; i < NR_NODES; i++)
		if (!list_empty(&zones[priority].nodes[i].free_pages))
			return &zones[priority].nodes[i];

	return NULL;
}

/*
 * Get free pages.
 */
struct page *__get_free_pages(size_t count, int priority)
{
	struct node *node;
	struct page *page;

	/* find free node */
	node = __find_free_node(count, priority);
	if (node)
		goto found;

	/* try to use kernel pages */
	if (priority != GFP_KERNEL) {
		node = __find_free_node(count, GFP_KERNEL);
		if (node)
			goto found;
	}

	/* reclaim pages */
	reclaim_pages();

	/* retry one last time */
	node = __find_free_node(count, GFP_KERNEL);
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
	node->zone->nr_free_pages -= node->order;

	/* add remaining pages to free list */
	if (count != node->order)
		__add_to_free_pages(page + count, node->order - count, priority);

	return page;
}

/*
 * Get a free page.
 */
struct page *__get_free_page(int priority)
{
	return __get_free_pages(1, priority);
}

/*
 * Free pages.
 */
void __free_pages(struct page *page, size_t count)
{
	if (!page)
		return;

	page->count--;
	if (!page->count) {
		page->inode = NULL;
		__add_to_free_pages(page, count, page->priority);
	}
}

/*
 * Free a page.
 */
void __free_page(struct page *page)
{
	__free_pages(page, 1);
}

/*
 * Get a free page.
 */
void *get_free_pages(size_t count)
{
	struct page *page;

	/* get a free page */
	page = __get_free_pages(count, GFP_KERNEL);
	if (!page)
		return NULL;

	/* make virtual address */
	return __va(page->page_nr * PAGE_SIZE);
}

/*
 * Get a free page.
 */
void *get_free_page()
{
	return get_free_pages(1);
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
 * Free a page.
 */
void free_page(void *address)
{
	return free_pages(address, 1);
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
	uint32_t page_array_end, addr, order, first_free_kpage, first_free_upage, i, j;

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
		for (j = 0, order = 1; j < NR_NODES; j++, order *= 2) {
			zones[i].nodes[j].zone = &zones[i];
			zones[i].nodes[j].order = order;
			INIT_LIST_HEAD(&zones[i].nodes[j].free_pages);
		}
	}

	/* add kernel and user free pages */
	__add_to_free_pages(&page_array[first_free_kpage], first_free_upage - first_free_kpage, GFP_KERNEL);
	if (nr_pages > first_free_upage)
		__add_to_free_pages(&page_array[first_free_upage], nr_pages - first_free_upage, GFP_USER);
}
