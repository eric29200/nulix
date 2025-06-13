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
 * Get a page from cache or create it.
 */
static struct page *grab_cache_page(struct inode *inode, off_t offset)
{
	struct page *page;

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

	return page;
}

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

	/* get page */
	page = grab_cache_page(inode, offset);
	if (!page)
		return NULL;

	/* page up to date */
	if (PageUptodate(page))
		return page;

	/* map page in kernel address space */
	kmap(page);

	/* read page */
	if (inode->i_op->readpage(inode, page)) {
		kunmap(page);
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
 * Read ahead some pages = try to grab maximum up to date ahead pages.
 */
static int generic_file_readahead(struct inode *inode, off_t page_offset, size_t max_pages)
{
	struct page *page, *pages_list[MAX_READ_AHEAD_PAGES];
	size_t nr_pages_read = 0, i;

	/* limit max pages */
	if (max_pages > MAX_READ_AHEAD_PAGES)
		max_pages = MAX_READ_AHEAD_PAGES;

	/* read */
	for (i = 0; i < max_pages; i++, page_offset += PAGE_SIZE) {
		/* get page */
		page = grab_cache_page(inode, page_offset);
		if (!page)
			break;

		/* page up to date */
		if (PageUptodate(page))
			goto next;

		/* map page in kernel address space */
		if (!kmap(page)) {
			__free_page(page);
			break;
		}

		/* read page */
		if (inode->i_op->readpage(inode, page)) {
			kunmap(page);
			__free_page(page);
			break;
		}

		/* page must be read */
		nr_pages_read++;

next:
		pages_list[i] = page;
	}

	/* no way to get a page */
	if (i == 0)
		return -EINVAL;

	/* execute block request if needed */
	if (nr_pages_read)
		execute_block_requests();

	/* release pages */
	while (i--) {
		kunmap(pages_list[i]);
		__free_page(pages_list[i]);
	}

	return 0;
}

/*
 * Generic file read.
 */
int generic_file_read(struct file *filp, char *buf, int count)
{
	struct inode *inode = filp->f_inode;
	int read = 0, err = 0, nr;
	off_t offset, page_offset;
	struct page *page;
	char *kaddr;
	size_t pos;

	/* fix number of characters to read */
	pos = filp->f_pos;
	if (pos + count > inode->i_size)
		count = inode->i_size - pos;

	/* read */
	while (count) {
		/* compute offset in page */
		page_offset = pos & PAGE_MASK;
		offset = pos & ~PAGE_MASK;
		nr = PAGE_SIZE - offset;
		if (nr > count)
			nr = count;

repeat:
		/* try to find page in cache */
		page = find_page(inode, page_offset);
		if (page)
			goto found_page;

		/* try to read some pages */
		err = generic_file_readahead(inode, page_offset, ((pos + count) >> PAGE_SHIFT) - (pos >> PAGE_SHIFT) + 1);
		if (err)
			break;

		goto repeat;
found_page:
		/* copy to user buffer */
		kaddr = kmap(page);
		memcpy(buf, kaddr + offset, nr);
		kunmap(page);

		/* release page */
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
	struct page *page;
	off_t offset;
	char *kaddr;

	/* handle append flag */
	if (filp->f_flags & O_APPEND)
		pos = inode->i_size;

	/* write page by page */
	while (count) {
		/* compute offset in page */
		offset = pos & ~PAGE_MASK;
		nr = PAGE_SIZE - offset;
		if (nr > count)
			nr = count;

		/* get page */
		page = grab_cache_page(inode, pos & PAGE_MASK);
		if (!page) {
			err = -ENOMEM;
			break;
		}

		/* prepare write page */
		kaddr = kmap(page);
		err = inode->i_op->prepare_write(inode, page, offset, offset + nr);
		if (!err) {
			/* write to page */
			memcpy(kaddr + offset, buf, nr);
			err = inode->i_op->commit_write(inode, page, offset, offset + nr);
		}

		/* release page */
		kunmap(page);
		__free_page(page);

		/* exit on error */
		if (err < 0)
			break;

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
