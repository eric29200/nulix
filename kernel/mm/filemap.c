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
	if (!kmap(page))
		goto err_kmap;

	/* read page */
	if (inode->i_op->readpage(inode, page))
		goto err_read;

	/* wait on page */
	execute_block_requests();

	return page;
err_read:
	kunmap(page);
err_kmap:
	__free_page(page);
	return NULL;
}

/*
 * Write a page.
 */
static int filemap_writepage(struct vm_area *vma, struct page *page)
{
	struct inode *inode = vma->vm_inode;
	int ret;

	/* map page in kernel address space */
	if (!kmap(page))
		return -ENOMEM;

	/* prepare write */
	ret = inode->i_op->prepare_write(inode, page, 0, PAGE_SIZE);
	if (ret) {
		kunmap(page);
		goto out;
	}

	/* commit write */
	ret = inode->i_op->commit_write(inode, page, 0, PAGE_SIZE);
out:
	return ret;
}

/*
 * Synchronize a memory region.
 */
static int filemap_sync_pte(pte_t *pte, struct vm_area *vma, uint32_t address, uint32_t flags)
{
	struct page *page;
	int ret;

	UNUSED(flags);

	if (pte_none(*pte))
		return 0;

	if (!pte_present(*pte))
		return 0;

	if (!pte_dirty(*pte))
		return 0;

	/* make pte clean */
	*pte = pte_mkclean(*pte);
	flush_tlb_page(vma->vm_mm->pgd, address);

	/* write page */
	page = pte_page(*pte);
	page->count++;
	ret = filemap_writepage(vma, page);
	__free_page(page);

	return ret;
}

/*
 * Synchronize a memory region.
 */
static int filemap_sync_pte_range(pmd_t *pmd, uint32_t address, size_t size,  struct vm_area *vma, uint32_t offset, uint32_t flags)
{
	uint32_t end;
	int ret = 0;
	pte_t *pte;

	if (pmd_none(*pmd))
		return 0;

	pte = pte_offset(pmd, address);
	offset += address & PMD_MASK;
	address &= ~PMD_MASK;
	end = address + size;
	if (end > PMD_SIZE)
		end = PMD_SIZE;

	do {
		ret |= filemap_sync_pte(pte, vma, address + offset, flags);
		address += PAGE_SIZE;
		pte++;
	} while (address < end);

	return ret;
}

/*
 * Synchronize a memory region.
 */
static int filemap_sync_pmd_range(pgd_t *pgd, uint32_t address, size_t size, struct vm_area *vma, uint32_t flags)
{
	uint32_t offset, end;
	int ret = 0;
	pmd_t *pmd;

	pmd = pmd_offset(pgd);
	offset = address & PGDIR_MASK;
	address &= ~PGDIR_MASK;
	end = address + size;
	if (end > PGDIR_SIZE)
		end = PGDIR_SIZE;

	do {
		ret |= filemap_sync_pte_range(pmd, address, end - address, vma, offset, flags);
		address = (address + PMD_SIZE) & PMD_MASK;
		pmd++;
	} while (address < end);

	return ret;
}

/*
 * Synchronize a memory region.
 */
static int filemap_sync(struct vm_area *vma, uint32_t address, size_t size, uint32_t flags)
{
	uint32_t end = address + size;
	int ret = 0;
	pgd_t *dir;

	dir = pgd_offset(vma->vm_mm->pgd, address);
	while (address < end) {
		ret |= filemap_sync_pmd_range(dir, address, end - address, vma, flags);
		address = (address + PGDIR_SIZE) & PGDIR_MASK;
		dir++;
	}

	flush_tlb(vma->vm_mm->pgd);

	return ret;
}

/*
 * Unmap a memory region.
 */
static void filemap_unmap(struct vm_area *vma, uint32_t start, size_t len)
{
	filemap_sync(vma, start, len, MS_ASYNC);
}

/*
 * Swap out a page.
 */
static int filemap_swapout(struct vm_area *vma, struct page *page)
{
	return filemap_writepage(vma, page);
}

/*
 * Private file mapping operations.
 */
static struct vm_operations file_shared_mmap = {
	.unmap		= filemap_unmap,
	.nopage		= filemap_nopage,
	.swapout	= filemap_swapout,
};

/*
 * Shared file mapping operations.
 */
static struct vm_operations file_private_mmap = {
	.nopage		= filemap_nopage,
};

/*
 * Generic mmap file.
 */
int generic_file_mmap(struct inode *inode, struct vm_area *vma)
{
	struct vm_operations *ops;

	/* choose operations */
	if ((vma->vm_flags & VM_SHARED) && (vma->vm_flags & VM_WRITE)) {
		ops = &file_shared_mmap;

		/* offset must be page aligned */
		if (vma->vm_offset & (PAGE_SIZE - 1))
			return -EINVAL;
	} else {
		ops = &file_private_mmap;

		/* offset must be block aligned */
		if (vma->vm_offset & (inode->i_sb->s_blocksize - 1))
			return -EINVAL;
	}

	/* inode must be a regular file */
	if (!inode->i_sb || !S_ISREG(inode->i_mode))
		return -EACCES;

	/* inode must implement readpage */
	if (!inode->i_op || !inode->i_op->readpage)
		return -ENOEXEC;

	/* update inode */
	inode->i_atime = CURRENT_TIME;
	inode->i_count++;
	mark_inode_dirty(inode);

	/* set memory region */
	vma->vm_inode = inode;
	vma->vm_ops = ops;

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
	while (i--)
		__free_page(pages_list[i]);

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
	mark_inode_dirty(inode);

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

	/* execute block requests */
	execute_block_requests();

	/* update position */
	filp->f_pos = pos;

	/* update inode */
	inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	mark_inode_dirty(inode);

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

/*
 * Shrink mmap = try to free a page.
 */
int shrink_mmap(int count)
{
	static size_t clock = 0;
	struct page *page;
	size_t i;

	for (i = 0; i < nr_pages; i++, clock++) {
		/* reset clock */
		if (clock >= nr_pages)
			clock = 0;

		/* skip used pages */
		page = &page_array[clock];
		if (page->count > 1)
			continue;

		/* skip shared memory pages */
		if (page->inode && page->inode->i_shm == 1)
			continue;

		/* is it a buffer cached page ? */
		if (page->buffers) {
			if (try_to_free_buffers(page))
				if (!--count)
					break;

			continue;
		}

		/* free cached page */
		if (page->inode) {
			remove_from_page_cache(page);
			__free_page(page);
			if (!--count)
				break;
		}
	}

	return count;
}