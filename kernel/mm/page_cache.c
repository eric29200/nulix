#include <mm/mm.h>
#include <fs/fs.h>
#include <stderr.h>

#define HASH_BITS 		11
#define HASH_SIZE 		(1 << HASH_BITS)

/* pages hash table */
static struct page *page_hash_table[HASH_SIZE];
uint32_t page_cache_size = 0;

/*
 * Hash an inode/offset.
 */
static inline uint32_t __page_hashfn(struct inode *inode, off_t offset)
{
#define i (((uint32_t) inode) / (sizeof(struct inode) & ~(sizeof(struct inode) - 1)))
#define o (offset >> PAGE_SHIFT)
#define s(x) ((x) + ((x) >> HASH_BITS))
	return s(i + o) & (HASH_SIZE - 1);
#undef i
#undef o
#undef s
}

/*
 * Get hash bucket.
 */
static inline struct page **page_hash(struct inode *inode, off_t offset)
{
	return page_hash_table + __page_hashfn(inode, offset);
}

/*
 * Find a page in hash table.
 */
struct page *find_page(struct inode *inode, off_t offset)
{
	struct page *page;

	for (page = *page_hash(inode, offset); page != NULL; page = page->next_hash) {
		if (page->inode == inode && page->offset == offset) {
			page->count++;
			return page;
		}
	}

	return NULL;
}

/*
 * Cache a page.
 */
void add_to_page_cache(struct page *page, struct inode *inode, off_t offset)
{
	struct page **p;

	/* set page */
	page->count++;
	page->inode = inode;
	page->offset = offset;

	/* insert page at the head */
	p = page_hash(inode, offset);
	page->prev_hash = NULL;
	page->next_hash = *p;

	/* update next page */
	if (*p)
		page->next_hash->prev_hash = page;

	/* update head */
	*p = page;

	/* add page to inode */
	list_add(&page->list, &inode->i_pages);

	/* update cache size */
	page_cache_size++;
}

/*
 * Remove a page from cache.
 */
void remove_from_page_cache(struct page *page)
{
	struct page *next_hash = page->next_hash, *prev_hash = page->prev_hash;
	struct page **p;

	/* unlink this page */
	page->next_hash = NULL;
	page->prev_hash = NULL;

	/* update next */
	if (next_hash)
		next_hash->prev_hash = prev_hash;

	/* update previous */
	if (prev_hash)
		prev_hash->next_hash = next_hash;

	/* update head */
	p = page_hash(page->inode, page->offset);
	if (*p == page)
		*p = next_hash;

	/* remove it from inode list */
	list_del(&page->list);

	/* update cache size */
	page_cache_size--;
}

/*
 * Init page cache.
 */
void  init_page_cache()
{
	int i;

	for (i = 0; i < HASH_SIZE; i++)
		page_hash_table[i] = NULL;
}
