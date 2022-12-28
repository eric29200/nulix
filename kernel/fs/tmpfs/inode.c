#include <fs/tmp_fs.h>
#include <proc/sched.h>
#include <fcntl.h>
#include <stderr.h>

/* next inode number */
static ino_t tmp_next_ino = TMPFS_ROOT_INO;

/*
 * Directory operations.
 */
static struct file_operations_t tmp_dir_fops = {
	.read			= tmp_file_read,
	.write			= tmp_file_write,
	.getdents64		= tmp_getdents64,
};

/*
 * File operations.
 */
static struct file_operations_t tmp_file_fops = {
	.read			= tmp_file_read,
	.write			= tmp_file_write,
	.mmap			= generic_file_mmap,
};

/*
 * Minix file inode operations.
 */
static struct inode_operations_t tmp_file_iops = {
	.fops			= &tmp_file_fops,
	.follow_link		= tmp_follow_link,
	.readlink		= tmp_readlink,
	.truncate		= tmp_truncate,
	.readpage		= tmp_readpage,
};

/*
 * Minix directory inode operations.
 */
static struct inode_operations_t tmp_dir_iops = {
	.fops			= &tmp_dir_fops,
	.lookup			= tmp_lookup,
	.create			= tmp_create,
	.link			= tmp_link,
	.unlink			= tmp_unlink,
	.symlink		= tmp_symlink,
	.mkdir			= tmp_mkdir,
	.rmdir			= tmp_rmdir,
	.rename			= tmp_rename,
	.mknod			= tmp_mknod,
	.truncate		= tmp_truncate,
};

/*
 * Create a new inode.
 */
struct inode_t *tmp_new_inode(struct super_block_t *sb, mode_t mode, dev_t dev)
{
	struct inode_t *inode;

	/* get an empty new inode */
	inode = get_empty_inode(sb);
	if (!inode)
		return NULL;

	/* set inode */
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	inode->i_ino = tmp_next_ino++;
	inode->i_ref = 1;
	inode->i_nlinks = 1;
	inode->i_sb = sb;
	inode->i_mode = mode;
	inode->i_uid = current_task->uid;
	inode->i_gid = current_task->gid;
	inode->i_rdev = dev;
	INIT_LIST_HEAD(&inode->u.tmp_i.i_pages);
	 
	/* set number of links */
	if (S_ISDIR(mode))
		inode->i_nlinks = 2;
	
	/* set operations */
	if (S_ISDIR(mode))
		inode->i_op = &tmp_dir_iops;
	else if (S_ISCHR(inode->i_mode))
		inode->i_op = char_get_driver(inode);
	else if (S_ISBLK(inode->i_mode))
		inode->i_op = block_get_driver(inode);
	else
		inode->i_op = &tmp_file_iops;

	/* hash inode */
	insert_inode_hash(inode);

	return inode;
}

/*
 * Read an inode.
 */
int tmp_read_inode(struct inode_t *inode)
{
	UNUSED(inode);
	return -ENOENT;
}

/*
 * Write an inode.
 */
int tmp_write_inode(struct inode_t *inode)
{
	/* nothing to do */
	UNUSED(inode);
	return 0;
}

/*
 * Put an inode.
 */
int tmp_put_inode(struct inode_t *inode)
{
	/* check inode */
	if (!inode)
		return -EINVAL;

	/* truncate and free inode */
	if (inode->i_ref <= 1 && !inode->i_nlinks) {
		inode->i_size = 0;
		tmp_truncate(inode);
		clear_inode(inode);
		return 0;
	}

	/* inodes must not be released, since they are only in memory */
	if (!inode->i_ref)
		inode->i_ref = 1;

	return 0;
}

/*
 * Grow an inode size.
 */
int tmp_inode_grow_size(struct inode_t *inode, size_t size)
{
	struct list_head_t pages_list, *pos, *n;
	struct page_t *page;
	int nb_pages, i;

	/* no need to grow */
	if (size <= inode->i_size)
		return 0;

	/* compute number of pages to add */
	nb_pages = (PAGE_ALIGN_UP(size) - PAGE_ALIGN_UP(inode->i_size)) / PAGE_SIZE;

	/* allocate pages */
	INIT_LIST_HEAD(&pages_list);
	for (i = 0; i < nb_pages; i++) {
		/* get a free page */
		page = __get_free_page();
		if (!page)
			goto err;

		/* memzero page */
		memset((void *) PAGE_ADDRESS(page), 0, PAGE_SIZE);

		/* add page to directory */
		list_del(&page->list);
		list_add_tail(&page->list, &pages_list);
	}

	/* add all pages to inode */
	list_for_each_safe(pos, n, &pages_list) {
		page = list_entry(pos, struct page_t, list);
		list_del(&page->list);
		list_add_tail(&page->list, &inode->u.tmp_i.i_pages);
	}

	/* set new inode size */
	inode->i_size = size;

	return 0;
err:
	/* free all pages */
	list_for_each_safe(pos, n, &pages_list) {
		page = list_entry(pos, struct page_t, list);
		__free_page(page);
	}

	return -ENOMEM;
}