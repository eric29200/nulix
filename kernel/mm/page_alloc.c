#include <mm/mm.h>
#include <fs/fs.h>

/* pages list */
static LIST_HEAD(free_pages);
static LIST_HEAD(used_pages);

/*
 * Get a free page.
 */
struct page *__get_free_page()
{
	struct page *page;

	/* try to get a page */
	if (list_empty(&free_pages)) {
		/* no more pages */
		reclaim_pages();

		if (list_empty(&free_pages))
			return NULL;
	}

	/* get first free page */
	page = list_first_entry(&free_pages, struct page, list);
	page->inode = NULL;
	page->offset = 0;
	page->buffers = NULL;
	page->count = 1;

	/* update lists */
	list_del(&page->list);
	list_add(&page->list, &used_pages);

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
		list_del(&page->list);
		list_add(&page->list, &free_pages);
	}
}

/*
 * Get a free page.
 */
void *get_free_page()
{
	struct page *page;

	/* get a free page */
	page = __get_free_page();
	if (!page)
		return NULL;

	/* make virtual address */
	return (void *) P2V(page->page * PAGE_SIZE);
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
		__free_page(&page_table[page_idx]);
}

/*
 * Reclaim pages, when memory is low.
 */
void reclaim_pages()
{
	struct page *page;
	uint32_t i;

	for (i = 0; i < nr_pages; i++) {
		page = &page_table[i];

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
			htable_delete(&page->htable);

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
	uint32_t i;

	for (i = 0; i < nr_pages; i++)
		if (page_table[i].count)
			list_add_tail(&page_table[i].list, &used_pages);
		else
			list_add_tail(&page_table[i].list, &free_pages);
}