#include <fs/fs.h>
#include <stdio.h>
#include <string.h>

/*
 * Memory zone.
 */
struct zone {
	struct list_head 	free_pages;
};

/* Memory zones */
static struct zone zones[NR_ZONES];

/*
 * Get a free page.
 */
struct page *__get_free_page(int priority)
{
	struct zone *zone;
	struct page *page;

	/* choose zone */
	zone = &zones[priority];
	if (list_empty(&zone->free_pages) && priority == GFP_USER)
		zone = &zones[GFP_KERNEL];

	/* no more free pages : reclaim pages */
	if (list_empty(&zone->free_pages))
		reclaim_pages();

	/* no way to get a page */
	if (list_empty(&zone->free_pages))
		return NULL;

	/* get first free page */
	page = list_first_entry(&zone->free_pages, struct page, list);
	page->inode = NULL;
	page->offset = 0;
	page->buffers = NULL;
	page->count = 1;

	/* remove page from free list */
	list_del(&page->list);

	return page;
}

/*
 * Free a page.
 */
void __free_page(struct page *page)
{
	if (!page)
		return;

	page->count--;
	if (!page->count) {
		page->inode = NULL;
		list_add(&page->list, &zones[page->priority].free_pages);
	}
}

/*
 * Get a free page.
 */
void *get_free_page()
{
	struct page *page;

	/* get a free page */
	page = __get_free_page(GFP_KERNEL);
	if (!page)
		return NULL;

	/* make virtual address */
	return __va(page->page_nr * PAGE_SIZE);
}

/*
 * Free a page.
 */
void free_page(void *address)
{
	uint32_t page_idx;

	/* get page index */
	page_idx = MAP_NR((uint32_t) address);

	/* free page */
	if (page_idx && page_idx < nr_pages)
		__free_page(&page_array[page_idx]);
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
void init_page_alloc(uint32_t last_kernel_addr)
{
	uint32_t page_array_end, addr, i;

	/* init memory zones */
	for (i = 0; i < NR_ZONES; i++)
		INIT_LIST_HEAD(&zones[i].free_pages);

	/* allocate global pages array */
	page_array = (struct page *) (KPAGE_START + PAGE_ALIGN_UP(last_kernel_addr));
	memset(page_array, 0, sizeof(struct page) * nr_pages);
	page_array_end = (uint32_t) page_array + sizeof(struct page) * nr_pages;

	/* kernel code pages */
	for (i = 0, addr = 0; i < nr_pages && (uint32_t) __va(addr) < page_array_end; i++, addr += PAGE_SIZE) {
		page_array[i].page_nr = i;
		page_array[i].count = 1;
		page_array[i].priority = GFP_KERNEL;
	}

	/* kernel free pages */
	for (; i < nr_pages && (uint32_t) __va(addr) < KPAGE_END; i++, addr += PAGE_SIZE) {
		page_array[i].page_nr = i;
		page_array[i].count = 0;
		page_array[i].priority = GFP_KERNEL;
		list_add_tail(&page_array[i].list, &zones[GFP_KERNEL].free_pages);
	}

	/* user free pages */
	for (; i < nr_pages; i++) {
		page_array[i].page_nr = i;
		page_array[i].count = 0;
		page_array[i].priority = GFP_USER;
		list_add_tail(&page_array[i].list, &zones[GFP_USER].free_pages);
	}
}
