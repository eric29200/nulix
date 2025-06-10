#include <mm/mmap.h>
#include <mm/paging.h>
#include <mm/highmem.h>
#include <drivers/block/blk_dev.h>
#include <proc/sched.h>
#include <fcntl.h>
#include <stderr.h>
#include <stdio.h>

#define MAX_READ_AHEAD_PAGES		32

/*
 * Handle a page fault = read page from file.
 */
static struct page *filemap_nopage(struct vm_area *vma, uint32_t address)
{
	struct inode *inode = vma->vm_inode;
	struct page *page;
	uint32_t offset;

	/* page align address */
	address = PAGE_ALIGN_DOWN(address);

	/* compute offset */
	offset = address - vma->vm_start + vma->vm_offset;
	if (offset >= inode->i_size)
		return NULL;

	/* try to get page from cache */
	page = find_page(inode, offset);
	if (page)
		return page;

	/* get a new page */
	page = __get_free_page(GFP_HIGHUSER);
	if (!page)
		return NULL;

	/* add it to cache */
	add_to_page_cache(page, inode, offset);

	/* map page in kernel address space */
	kmap(page);

	/* read page */
	if (inode->i_op->readpage(inode, page)) {
		kunmap(page);
		remove_from_page_cache(page);
		__free_page(page);
		return NULL;
	}

	/* wait on page */
	execute_block_requests();

	/* unmap page */
	kunmap(page);

	return page;
}

/*
 * Open a memory region.
 */
void filemap_open(struct vm_area *vma)
{
	vma->vm_inode->i_count++;
	list_add(&vma->list_share, &vma->vm_inode->i_mmap);
}

/*
 * Close a memory region.
 */
void filemap_close(struct vm_area *vma)
{
	list_del(&vma->list_share);
	iput(vma->vm_inode);
}

/*
 * File mapping operations.
 */
static struct vm_operations file_mmap = {
	.open		= filemap_open,
	.close		= filemap_close,
	.nopage		= filemap_nopage,
};

/*
 * Generic mmap file.
 */
int generic_file_mmap(struct inode *inode, struct vm_area *vma)
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
	inode->i_count++;

	/* set memory region */
	vma->vm_inode = inode;
	vma->vm_ops = &file_mmap;

	return 0;
}

/*
 * Read ahead some pages.
 */
static void generic_file_readahead(struct inode *inode, size_t pos, int count)
{
	struct page *page, *pages_list[MAX_READ_AHEAD_PAGES];
	size_t pages_count = 0, i;
	off_t offset;
	int nr;

	/* read */
	while (count && pos < inode->i_size) {
		/* compute offset in page */
		offset = pos & ~PAGE_MASK;
		nr = PAGE_SIZE - offset;

		/* try to find page in cache */
		page = find_page(inode, pos & PAGE_MASK);
		if (page)
			goto next;

		/* get a free page */
		page = __get_free_page(GFP_HIGHUSER);
		if (!page)
			break;

		/* map page in kernel address space */
		if (!kmap(page)) {
			__free_page(page);
			break;
		}

		/* add it to cache */
		add_to_page_cache(page, inode, pos & PAGE_MASK);

		/* read page */
		if (inode->i_op->readpage(inode, page)) {
			kunmap(page);
			remove_from_page_cache(page);
			__free_page(page);
			break;
		}

		/* page must be read */
		pages_list[pages_count++] = page;
		if (pages_count == MAX_READ_AHEAD_PAGES)
			break;
next:
		/* adjust size */
		if (nr > count)
			nr = count;
		if (pos + nr > inode->i_size)
			nr = inode->i_size - pos;

		/* update sizes */
		pos += nr;
		count -= nr;
	}

	/* nothing to read */
	if (!pages_count)
		return;

	/* execute block request */
	execute_block_requests();

	/* release pages */
	for (i = 0; i < pages_count; i++) {
		kunmap(pages_list[i]);
		__free_page(pages_list[i]);
	}
}

/*
 * Generic file read.
 */
int generic_file_read(struct file *filp, char *buf, int count)
{
	struct inode *inode = filp->f_inode;
	int read = 0, err = 0, nr;
	struct page *page;
	off_t offset;
	char *kaddr;
	size_t pos;

	/* read */
	for (pos = filp->f_pos; count && pos < inode->i_size;) {
		/* compute offset in page */
		offset = pos & ~PAGE_MASK;
		nr = PAGE_SIZE - offset;
		kaddr = NULL;

repeat:
		/* try to find page in cache */
		page = find_page(inode, pos & PAGE_MASK);
		if (page)
			goto found_page;

		/* try to read some pages */
		generic_file_readahead(inode, pos, count);
		goto repeat;
found_page:
		/* adjust size */
		if (nr > count)
			nr = count;
		if (pos + nr > inode->i_size)
			nr = inode->i_size - pos;

		/* map page in kernel address space */
		kaddr = kmap(page);
		if (!kaddr) {
			__free_page(page);
			err = -ENOMEM;
			break;
		}

		/* copy to user buffer */
		memcpy(buf, kaddr + offset, nr);

		/* release page */
		kunmap(page);
		__free_page(page);

		/* update sizes */
		buf += nr;
		pos += nr;
		read += nr;
		count -= nr;
	}

	/* update position */
	filp->f_pos = pos;

	/* update inode */
	inode->i_atime = CURRENT_TIME;
	inode->i_dirt = 1;

	return read ? read : err;
}

/*
 * Generic file write.
 */
int generic_file_write(struct file *filp, const char *buf, int count)
{
	struct inode *inode = filp->f_inode;
	int written = 0, err = 0, nr;
	size_t pos = filp->f_pos;
	int partial, new_page;
	struct page *page;
	off_t offset;

	/* handle append flag */
	if (filp->f_flags & O_APPEND)
		pos = inode->i_size;

	/* write page by page */
	while (count) {
		new_page = 0;

		/* compute offset in page */
		offset = pos & ~PAGE_MASK;
		nr = PAGE_SIZE - offset;
		if (nr > count)
			nr = count;

		/* try to find page in cache */
		page = find_page(inode, pos & PAGE_MASK);
		if (page)
			goto found_page;

		/* get a free page */
		page = __get_free_page(GFP_HIGHUSER);
		if (!page) {
			err = -ENOMEM;
			break;
		}

		/* add it to cache */
		add_to_page_cache(page, inode, pos & PAGE_MASK);
		new_page = 1;

found_page:
		/* map page in kernel address space */
		if (!kmap(page)) {
			err = -ENOMEM;
			__free_page(page);
			break;
		}

		/* write page */
		err = inode->i_op->writepage(inode, page, offset, nr, buf, &partial);

		/* release page */
		kunmap(page);
		__free_page(page);

		/* exit on error */
		if (err < 0)
			break;

		/* partial page : remove it from cache */
		if (partial && new_page) {
			remove_from_page_cache(page);
			__free_page(page);
		}

		/* update sizes */
		buf += nr;
		pos += nr;
		written += nr;
		count -= nr;
		if (pos > inode->i_size)
			inode->i_size = pos;
	}

	/* update position */
	filp->f_pos = pos;

	/* update inode */
	inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	inode->i_dirt = 1;

	return written ? written : err;
}

/*
 * Truncate inode pages.
 */
void truncate_inode_pages(struct inode *inode, off_t start)
{
	struct list_head *pos, *n;
	struct page *page;
	off_t offset;

	list_for_each_safe(pos, n, &inode->i_pages) {
		page = list_entry(pos, struct page, list);
		offset = page->offset;

		/* full page truncate */
		if (offset >= start) {
			remove_from_page_cache(page);
			__free_page(page);
			continue;
		}

		/* partial page truncate */
		offset = start - offset;
		if (offset < PAGE_SIZE)
			clear_user_highpage_partial(page, offset);
	}
}
