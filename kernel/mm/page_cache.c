#include <mm/mm.h>
#include <fs/fs.h>
#include <stderr.h>

#define HASH_BITS			12
#define HASH_SIZE			(1 << HASH_BITS)

/* pages hash table */
static struct htable_link **page_htable = NULL;

/*
 * Hash an inode/offset.
 */
static inline uint32_t __hashfn(struct inode *inode, off_t offset)
{
#define i (((uint32_t) inode) / (sizeof(struct inode) & ~(sizeof(struct inode) - 1)))
#define o (offset >> PAGE_SHIFT)
#define s(x) ((x) + ((x) >> HASH_BITS))
	return s(i + o) & (HASH_SIZE);
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
	node = htable_lookup(page_htable, __hashfn(inode, offset), HASH_BITS);
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
	htable_insert(page_htable, &page->htable, __hashfn(inode, offset), HASH_BITS);

	/* add page to inode */
	list_del(&page->list);
	list_add(&page->list, &inode->i_pages);
}

/*
 * Init page cache.
 */
int init_page_cache()
{
	/* allocate page hash table */
	page_htable = (struct htable_link **) kmalloc(sizeof(struct htable_link *) * HASH_SIZE);
	if (!page_htable)
		return -ENOMEM;
	memset(page_htable, 0, sizeof(struct htable_link *) * HASH_SIZE);

	/* init page hash table */
	htable_init(page_htable, HASH_BITS);

	return 0;
}
