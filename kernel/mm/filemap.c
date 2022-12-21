#include <mm/mmap.h>
#include <mm/paging.h>
#include <proc/sched.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Read a page.
 */
static int generic_readpage(struct inode_t *inode, struct page_t *page, off_t offset)
{
	struct file_t filp;
	int ret;

	/* set temporary file */
	filp.f_mode = O_RDONLY;
	filp.f_flags = 0;
	filp.f_pos = offset;
	filp.f_ref = 1;
	filp.f_inode = inode;
	filp.f_op = inode->i_op->fops;

	/* read page */
	ret = do_read(&filp, (char *) PAGE_ADDRESS(page), PAGE_SIZE);
	if (ret < 0)
		return ret;

	return 0;
}

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

	/* read page */
	page = &page_table[MAP_NR(new_page)];
	ret = generic_readpage(inode, page, offset);
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
