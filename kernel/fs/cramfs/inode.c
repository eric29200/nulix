#include <fs/fs.h>
#include <fs/cramfs_fs.h>
#include <mm/highmem.h>
#include <stdio.h>
#include <fcntl.h>

/*
 * Directory operations.
 */
struct file_operations cramfs_dir_fops = {
	.readdir		= cramfs_readdir,
};

/*
 * File operations.
 */
struct file_operations cramfs_file_fops = {
	.read			= generic_file_read,
	.mmap			= generic_file_mmap,
};

/*
 * Cramfs directory inode operations.
 */
struct inode_operations cramfs_dir_iops = {
	.fops			= &cramfs_dir_fops,
	.lookup			= cramfs_lookup,
};

/*
 * Read a page.
 */
static int cramfs_readpage(struct inode *inode, struct page *page)
{
	uint32_t max_block, start_offset, blkptr_offset, compr_len, bytes_filled = 0;
	struct super_block *sb = inode->i_sb;

	/* lock page */
	LockPage(page);

	max_block = (inode->i_size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	if (page->offset < max_block) {
		/* get offset on disk */
		blkptr_offset = CRAMOFFSET(inode) + page->offset * 4;
		start_offset = CRAMOFFSET(inode) + max_block * 4;
		if (page->offset)
			start_offset = *(uint32_t *) cramfs_read(sb, blkptr_offset - 4, 4);

		/* read compressed length */
		compr_len = (*(uint32_t *) cramfs_read(sb, blkptr_offset, 4) - start_offset);

		/* read and uncompress data */
		if (compr_len)
			bytes_filled = cramfs_uncompress_block(cramfs_read(sb, start_offset, compr_len), compr_len, (void *) page_address(page), PAGE_SIZE);
	}

	/* memzero end of page */
	memset((void *) (page_address(page) + bytes_filled), 0, PAGE_SIZE - bytes_filled);

	/* set page up to date*/
	SetPageUptodate(page);
	UnlockPage(page);
	kunmap(page);

	return 0;
}

/*
 * Cramfs file inode operations.
 */
struct inode_operations cramfs_file_iops = {
	.fops			= &cramfs_file_fops,
	.readpage		= cramfs_readpage,
};

/*
 * Get a cramfs inode.
 */
struct inode *cramfs_get_inode(struct super_block *sb, struct cramfs_inode *cramfs_inode, uint32_t offset)
{
	struct inode *inode;

	/* get an empty inode */
	inode = get_empty_inode(sb);
	if (!inode)
		return NULL;

	/* set inode */
	inode->i_mode = cramfs_inode->mode;
	inode->i_uid = cramfs_inode->uid;
	inode->i_size = cramfs_inode->size;
	inode->i_gid = cramfs_inode->gid;
	inode->i_ino = cramino(cramfs_inode, offset);
	inode->i_sb = sb;
	inode->i_dev = sb->s_dev;
	inode->i_nlinks = 1;

	/* set operations */
	if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &cramfs_dir_iops;
	} else if (S_ISCHR(inode->i_mode)) {
		inode->i_rdev = cramfs_inode->size;
		inode->i_op = &chrdev_iops;
	} else if (S_ISBLK(inode->i_mode)) {
		inode->i_rdev = cramfs_inode->size;
		inode->i_op = &blkdev_iops;
	} else if (S_ISFIFO(inode->i_mode)) {
		init_fifo(inode);
	} else {
		inode->i_op = &cramfs_file_iops;
	}

	return inode;
}