#include <fs/dev_fs.h>
#include <fcntl.h>

/*
 * Read a file.
 */
int devfs_file_read(struct file_t *filp, char *buf, int count)
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
	list_for_each(pos, &filp->f_inode->u.dev_i.i_pages) {
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
