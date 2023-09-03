#include <fs/tmp_fs.h>
#include <proc/sched.h>
#include <fcntl.h>
#include <stderr.h>

/* next inode number */
static ino_t tmpfs_next_ino = TMPFS_ROOT_INO;

/*
 * Directory operations.
 */
static struct file_operations tmpfs_dir_fops = {
	.getdents64		= tmpfs_getdents64,
};

/*
 * File operations.
 */
static struct file_operations tmpfs_file_fops = {
	.read			= tmpfs_file_read,
	.write			= tmpfs_file_write,
	.mmap			= generic_file_mmap,
};

/*
 * Symbolic link inode operations.
 */
static struct inode_operations tmpfs_symlink_iops = {
	.follow_link		= tmpfs_follow_link,
	.readlink		= tmpfs_readlink,
};

/*
 * File inode operations.
 */
static struct inode_operations tmpfs_file_iops = {
	.fops			= &tmpfs_file_fops,
	.truncate		= tmpfs_truncate,
	.readpage		= tmpfs_readpage,
};

/*
 * Directory inode operations.
 */
static struct inode_operations tmpfs_dir_iops = {
	.fops			= &tmpfs_dir_fops,
	.lookup			= tmpfs_lookup,
	.create			= tmpfs_create,
	.link			= tmpfs_link,
	.unlink			= tmpfs_unlink,
	.symlink		= tmpfs_symlink,
	.mkdir			= tmpfs_mkdir,
	.rmdir			= tmpfs_rmdir,
	.rename			= tmpfs_rename,
	.mknod			= tmpfs_mknod,
	.truncate		= tmpfs_truncate,
};

/*
 * Create a new inode.
 */
struct inode *tmpfs_new_inode(struct super_block *sb, mode_t mode, dev_t dev)
{
	struct inode *inode;

	/* get an empty new inode */
	inode = get_empty_inode(sb);
	if (!inode)
		return NULL;

	/* set inode */
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	inode->i_ino = tmpfs_next_ino++;
	inode->i_ref = 1;
	inode->i_nlinks = 1;
	inode->i_sb = sb;
	inode->i_mode = mode;
	inode->i_uid = current_task->uid;
	inode->i_gid = current_task->gid;
	inode->i_rdev = dev;
	INIT_LIST_HEAD(&inode->u.tmp_i.i_pages);
	inode->u.tmp_i.i_shmid = -1;
	 
	/* set number of links */
	if (S_ISDIR(mode))
		inode->i_nlinks = 2;

	/* set operations */
	if (S_ISDIR(mode))
		inode->i_op = &tmpfs_dir_iops;
	else if (S_ISLNK(mode))
		inode->i_op = &tmpfs_symlink_iops;
	else if (S_ISCHR(inode->i_mode))
		inode->i_op = char_get_driver(inode);
	else if (S_ISBLK(inode->i_mode))
		inode->i_op = block_get_driver(inode);
	else
		inode->i_op = &tmpfs_file_iops;

	/* hash inode */
	insert_inode_hash(inode);

	return inode;
}

/*
 * Read an inode.
 */
int tmpfs_read_inode(struct inode *inode)
{
	UNUSED(inode);
	return -ENOENT;
}

/*
 * Write an inode.
 */
int tmpfs_write_inode(struct inode *inode)
{
	/* nothing to do */
	UNUSED(inode);
	return 0;
}

/*
 * Put an inode.
 */
int tmpfs_put_inode(struct inode *inode)
{
	/* check inode */
	if (!inode)
		return -EINVAL;

	/* truncate and free inode */
	if (inode->i_ref <= 1 && !inode->i_nlinks) {
		inode->i_size = 0;
		tmpfs_truncate(inode);
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
int tmpfs_inode_grow_size(struct inode *inode, size_t size)
{
	struct list_head pages_list, *pos, *n;
	struct page *page;
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
		page = list_entry(pos, struct page, list);
		list_del(&page->list);
		list_add_tail(&page->list, &inode->u.tmp_i.i_pages);
	}

	/* set new inode size */
	inode->i_size = size;

	return 0;
err:
	/* free all pages */
	list_for_each_safe(pos, n, &pages_list) {
		page = list_entry(pos, struct page, list);
		__free_page(page);
	}

	return -ENOMEM;
}
