#include <fs/tmp_fs.h>
#include <mm/highmem.h>

/*
 * Truncate an inode.
 */
void tmpfs_truncate(struct inode *inode)
{
	struct list_head *pos, *n;
	struct page *page;
	size_t offset = 0;

	list_for_each_safe(pos, n, &inode->u.tmp_i.i_pages) {
		page = list_entry(pos, struct page, list);

		/* full/partial page free */
		if (offset >= inode->i_size) {
			list_del(&page->list);
			__free_page(page);
		} else if (offset + PAGE_SIZE > inode->i_size) {
			clear_user_highpage_partial(page, inode->i_size - offset);
		}

		/* update offset */
		offset += PAGE_SIZE;
	}
}
