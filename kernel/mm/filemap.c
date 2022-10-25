#include <mm/mmap.h>
#include <mm/paging.h>
#include <proc/sched.h>
#include <fcntl.h>
#include <stderr.h>

/*
 * Handle a page fault = read page from file.
 */
static int filemap_nopage(struct vm_area_t *vma, uint32_t address)
{
	struct inode_t *inode = vma->vm_inode;
	struct file_t filp;
	off_t offset;
	int ret;

	/* compute offset */
	offset = PAGE_ALIGN_DOWN(address) - vma->vm_start + vma->vm_offset;
	if (offset >= inode->i_size)
		return -EINVAL;

	/* set temporary file */
	filp.f_mode = O_RDONLY;
	filp.f_flags = 0;
	filp.f_pos = offset;
	filp.f_ref = 1;
	filp.f_inode = inode;
	filp.f_op = inode->i_op->fops;

	/* read page */
	ret = do_read(&filp, (char *) PAGE_ALIGN_DOWN(address), PAGE_SIZE);
	if (ret < 0)
		return ret;

	return 0;
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
	/* shared mode is not supported */
	if (vma->vm_flags & VM_SHARED)
		return -EINVAL;

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
