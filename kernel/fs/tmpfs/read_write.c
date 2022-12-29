#include <fs/tmp_fs.h>
#include <fcntl.h>

/*
 * Read a file.
 */
int tmpfs_file_read(struct file_t *filp, char *buf, int count)
{
	size_t page_offset, offset, nb_chars, left;
	struct list_head_t *pos;
	struct page_t *page;

	/* adjust size */
	if (filp->f_pos + count > filp->f_inode->i_size)
		count = filp->f_inode->i_size - filp->f_pos;

	if (count <= 0)
		return 0;

	/* walk through all pages */
	page_offset = 0;
	left = count;
	list_for_each(pos, &filp->f_inode->u.tmp_i.i_pages) {
		page = list_entry(pos, struct page_t, list);
		if (page_offset + PAGE_SIZE < filp->f_pos)
			continue;

		/* compute offset in page and number of characters to read */
		offset = filp->f_pos - page_offset;
		nb_chars = PAGE_SIZE - offset;
		if (nb_chars > left)
			nb_chars = left;

		/* copy data */
		memcpy(buf, (void *) PAGE_ADDRESS(page) + offset, nb_chars);

		/* update sizes */
		filp->f_pos += nb_chars;
		buf += nb_chars;
		left -= nb_chars;
		page_offset += PAGE_SIZE;

		/* end of read */
		if (left <= 0)
			break;
	}

	return count - left;
}

/*
 * Write to a file.
 */
int tmpfs_file_write(struct file_t *filp, const char *buf, int count)
{
	size_t page_offset, left, offset, nb_chars;
	struct list_head_t *pos;
	struct page_t *page;

	/* handle append flag */
	if (filp->f_flags & O_APPEND)
		filp->f_pos = filp->f_inode->i_size;

	/* grow inode size */
	tmpfs_inode_grow_size(filp->f_inode, filp->f_pos + count);

	/* walk through all pages */
	page_offset = 0;
	left = count;
	list_for_each(pos, &filp->f_inode->u.tmp_i.i_pages) {
		page = list_entry(pos, struct page_t, list);
		if (page_offset + PAGE_SIZE < filp->f_pos)
			continue;

		/* compute offset in page and number of characters to write */
		offset = filp->f_pos - page_offset;
		nb_chars = PAGE_SIZE - offset;
		if (nb_chars > left)
			nb_chars = left;

		/* copy data */
		memcpy((void *) PAGE_ADDRESS(page) + offset, buf, nb_chars);

		/* update sizes */
		filp->f_pos += nb_chars;
		buf += nb_chars;
		left -= nb_chars;
		page_offset += PAGE_SIZE;

		/* end of write */
		if (left <= 0)
			break;
	}

	return count - left;
}

/*
 * Read a page.
 */
int tmpfs_readpage(struct inode_t *inode, struct page_t *page)
{
	struct page_t *inode_page;
	struct list_head_t *pos;
	off_t offset = 0;

	list_for_each(pos, &inode->u.tmp_i.i_pages) {
		/* skip pages until offset */
		inode_page = list_entry(pos, struct page_t, list);
		if (offset != page->offset) {
			offset += PAGE_SIZE;
			continue;
		}

		/* copy data */
		memcpy((void *) PAGE_ADDRESS(page), (void *) PAGE_ADDRESS(inode_page), PAGE_SIZE);
		break;
	}

	return 0;
}
