#include <mm/mmap.h>
#include <mm/paging.h>
#include <proc/sched.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Fill a page.
 */
static struct page_t *fill_page(struct inode_t *inode, off_t offset)
{
	struct page_t *page;
	uint32_t new_page;
	int ret;

	/* get a new page */
	new_page = (uint32_t) get_free_page();
	if (!new_page)
		return NULL;

	/* get page */
	page = &page_table[MAP_NR(new_page)];
	page->inode = inode;
	page->offset = offset;

	/* read page */
	ret = inode->i_op->readpage(inode, page);
	if (ret) {
		free_page((void *) new_page);
		return NULL;
	}

	return page;
}

/*
 * Handle a page fault = read page from file.
 */
static struct page_t *filemap_nopage(struct vm_area_t *vma, uint32_t address)
{
	struct inode_t *inode = vma->vm_inode;
	off_t offset;

	/* page align address */
	address = PAGE_ALIGN_DOWN(address);

	/* compute offset */
	offset = address - vma->vm_start + vma->vm_offset;
	if (offset >= inode->i_size)
		return NULL;

	/* fill in page */
	return fill_page(inode, offset);
}

/*
 * Private file mapping operations.
 */
static struct vm_operations_t file_private_mmap = {
	.nopage		= filemap_nopage,
};

/*
 * Generic mmap file.
 */
int generic_file_mmap(struct inode_t *inode, struct vm_area_t *vma)
{
	/* inode must be a regular file */
	if (!inode->i_sb || !S_ISREG(inode->i_mode))
		return -EACCES;

	/* offset must be block aligned */
	if (vma->vm_offset & (inode->i_sb->s_blocksize - 1))
		return -EINVAL;

	/* update inode */
	inode->i_atime = CURRENT_TIME;
	inode->i_dirt = 1;
	inode->i_ref++;

	/* set memory region */
	vma->vm_inode = inode;
	vma->vm_ops = &file_private_mmap;

	return 0;
}

/*
 * Generic file read.
 */
int generic_file_read(struct file_t *filp, char *buf, int count)
{
	struct inode_t *inode = filp->f_inode;
	off_t index, offset;
	struct page_t *page;
	uint32_t page_buf;
	int left, ret, nr;

	/* adjust size */
	if (filp->f_pos + count > filp->f_inode->i_size)
		count = filp->f_inode->i_size - filp->f_pos;
	if (count <= 0)
		return 0;

	/* compute page index and offset */
	index = filp->f_pos >> PAGE_SHIFT;
	offset = filp->f_pos & ~PAGE_MASK;

	/* read page by page */
	for (left = count; left > 0; index++, offset = 0) {
		/* get a free page */
		page_buf = (uint32_t) get_free_page();
		if (!page_buf)
			break;

		/* get page */
		page = &page_table[MAP_NR(page_buf)];
		page->inode = inode;
		page->offset = index * PAGE_SIZE;

		/* read page */
		ret = inode->i_op->readpage(inode, page);
		if (ret)
			break;

		/* compute number of characters to read */
		nr = PAGE_SIZE - offset;
		if (nr > left)
			nr = left;

		/* copy data to user buffer */
		memcpy(buf, (void *) page_buf + offset, nr);
		buf += nr;

		/* update sizes */
		filp->f_pos += nr;
		left -= nr;
	}

	return count - left;
}

/*
 * Write dirty pages.
 */
int filemap_fdatasync(struct address_space_t *mapping)
{
	struct list_head_t *pos, *n;
	struct page_t *page;
	int ret = 0, err;

	/* writepage not implemented */
	if (!mapping->inode->i_op->writepage)
		return -EINVAL;

	/* for each dirty page */
	list_for_each_safe(pos, n, &mapping->dirty_pages) {
		page = list_entry(pos, struct page_t, list);

		/* write page */
		err = mapping->inode->i_op->writepage(page);
		if (err && !ret)
			ret = err;

		/* put it in clean list */
		list_del(&page->list);
		list_add(&page->list, &mapping->clean_pages);
	}

	return ret;
}