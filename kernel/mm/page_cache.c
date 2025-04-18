#include <mm/mm.h>
#include <fs/fs.h>
#include <stderr.h>

#define PAGE_HASH_BITS			11
#define PAGE_HASH_SIZE			(1 << PAGE_HASH_BITS)

/* pages hash table */
static struct htable_link **page_htable = NULL;

/*
 * Hash an inode/offset.
 */
static inline uint32_t __page_hashfn(struct inode *inode, off_t offset)
{
#define i (((uint32_t) inode) / (sizeof(struct inode) & ~(sizeof(struct inode) - 1)))
#define o (offset >> PAGE_SHIFT)
#define s(x) ((x) + ((x) >> PAGE_HASH_BITS))
	return s(i + o) & (PAGE_HASH_SIZE);
#undef i
#undef o
#undef s
}

/*
 * Find a page in hash table.
 */
struct page *find_page(struct inode *inode, off_t offset)
{
	struct htable_link *node;
	struct page *page;

	/* try to find buffer in cache */
	node = htable_lookup(page_htable, __page_hashfn(inode, offset), PAGE_HASH_BITS);
	while (node) {
		page = htable_entry(node, struct page, htable);
		if (page->inode == inode && page->offset == offset) {
			page->count++;
			return page;
		}

		node = node->next;
	}

	return NULL;
}

/*
 * Cache a page.
 */
void add_to_page_cache(struct page *page, struct inode *inode, off_t offset)
{
	/* set page */
	page->count++;
	page->inode = inode;
	page->offset = offset;

	/* cache page */
	htable_delete(&page->htable);
	htable_insert(page_htable, &page->htable, __page_hashfn(inode, offset), PAGE_HASH_BITS);

	/* add page to inode */
	list_del(&page->list);
	list_add(&page->list, &inode->i_pages);
}

/*
 * Init page cache.
 */
int init_page_cache()
{
	uint32_t addr, nr, i;

	/* allocate page hash table */
	nr = 1 + nr_pages * sizeof(struct htable_link *) / PAGE_SIZE;
	for (i = 0; i < nr; i++) {
		/* get a free page */
		addr = (uint32_t) get_free_page();
		if (!addr)
			return -ENOMEM;

		/* reset page */
		memset((void *) addr, 0, PAGE_SIZE);

		/* set buffer hash table */
		if (i == 0)
			page_htable = (struct htable_link **) addr;
	}

	return 0;
}
