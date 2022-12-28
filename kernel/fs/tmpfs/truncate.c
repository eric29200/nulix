#include <fs/tmp_fs.h>

/*
 * Truncate an inode.
 */
void tmp_truncate(struct inode_t *inode)
{
	struct list_head_t *pos, *n;
	struct page_t *page;
	size_t offset = 0;

	list_for_each_safe(pos, n, &inode->u.tmp_i.i_pages) {
		page = list_entry(pos, struct page_t, list);

		/* full/partial page free */
		if (offset >= inode->i_size)
			__free_page(page);
		else if (offset + PAGE_SIZE > inode->i_size)
			memset((void *) PAGE_ADDRESS(page) + inode->i_size - offset, 0, offset + PAGE_SIZE - inode->i_size);

		/* update offset */
		offset += PAGE_SIZE;
	}
}
