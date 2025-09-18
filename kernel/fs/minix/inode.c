#include <fs/fs.h>
#include <fs/minix_fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <drivers/char/tty.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <string.h>

/*
 * Directory operations.
 */
struct file_operations minix_dir_fops = {
	.getdents64		= minix_getdents64,
};

/*
 * File operations.
 */
struct file_operations minix_file_fops = {
	.read			= generic_file_read,
	.write			= generic_file_write,
	.mmap			= generic_file_mmap,
};

/*
 * Minix symbolic link inode operations.
 */
struct inode_operations minix_symlink_iops = {
	.follow_link		= minix_follow_link,
	.readlink		= minix_readlink,
};

/*
 * Minix file inode operations.
 */
struct inode_operations minix_file_iops = {
	.fops			= &minix_file_fops,
	.truncate		= minix_truncate,
	.get_block		= minix_get_block,
	.bmap			= generic_block_bmap,
	.readpage		= generic_readpage,
	.prepare_write		= generic_prepare_write,
	.commit_write		= generic_commit_write,
};

/*
 * Minix directory inode operations.
 */
struct inode_operations minix_dir_iops = {
	.fops			= &minix_dir_fops,
	.lookup			= minix_lookup,
	.create			= minix_create,
	.link			= minix_link,
	.unlink			= minix_unlink,
	.symlink		= minix_symlink,
	.mkdir			= minix_mkdir,
	.rmdir			= minix_rmdir,
	.rename			= minix_rename,
	.mknod			= minix_mknod,
	.truncate		= minix_truncate,
};

/*
 * Read a Minix V2 inode on disk.
 */
int minix_read_inode(struct inode *inode)
{
	struct minix_sb_info *sbi = minix_sb(inode->i_sb);
	struct minix2_inode *raw_inode;
	struct buffer_head *bh;
	int inodes_per_block, i;
	uint32_t block;

	/* check inode number */
	if (!inode->i_ino || inode->i_ino > sbi->s_ninodes)
		return -EINVAL;

	/* compute inode store block */
	inodes_per_block = inode->i_sb->s_blocksize / sizeof(struct minix2_inode);
	block = 2 + sbi->s_imap_blocks + sbi->s_zmap_blocks + (inode->i_ino - 1) / inodes_per_block;

	/* read inode store block */
	bh = bread(inode->i_dev, block, inode->i_sb->s_blocksize);
	if (!bh) {
		iput(inode);
		return -EIO;
	}

	/* get raw inode */
	raw_inode = &((struct minix2_inode *) bh->b_data)[(inode->i_ino - 1) % inodes_per_block];

	/* set inode */
	inode->i_mode = raw_inode->i_mode;
	inode->i_nlinks = raw_inode->i_nlinks;
	inode->i_uid = raw_inode->i_uid;
	inode->i_gid = raw_inode->i_gid;
	inode->i_size = raw_inode->i_size;
	inode->i_atime = raw_inode->i_atime;
	inode->i_ctime = raw_inode->i_ctime;
	inode->i_mtime = raw_inode->i_mtime;
	inode->i_blocks = 0;
	for (i = 0; i < 10; i++)
		inode->u.minix_i.i_zone[i] = raw_inode->i_zone[i];

	/* set operations */
	if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &minix_dir_iops;
	} else if (S_ISLNK(inode->i_mode)) {
		inode->i_op = &minix_symlink_iops;
	} else if (S_ISCHR(inode->i_mode)) {
		inode->i_rdev = inode->u.minix_i.i_zone[0];
		inode->i_op = &chrdev_iops;
	} else if (S_ISBLK(inode->i_mode)) {
		inode->i_rdev = inode->u.minix_i.i_zone[0];
		inode->i_op = &blkdev_iops;
	} else {
		inode->i_op = &minix_file_iops;
	}

	/* release block buffer */
	brelse(bh);

	return 0;
}

/*
 * Write a Minix V2/V3 inode on disk.
 */
int minix_write_inode(struct inode *inode)
{
	struct minix_sb_info *sbi = minix_sb(inode->i_sb);
	struct minix2_inode *raw_inode;
	struct buffer_head *bh;
	int inodes_per_block, i;
	uint32_t block;

	/* compute inode store block */
	inodes_per_block = inode->i_sb->s_blocksize / sizeof(struct minix2_inode);
	block = 2 + sbi->s_imap_blocks + sbi->s_zmap_blocks + (inode->i_ino - 1) / inodes_per_block;

	/* read inode store block */
	bh = bread(inode->i_dev, block, inode->i_sb->s_blocksize);
	if (!bh) {
		iput(inode);
		return -EIO;
	}

	/* get raw inode */
	raw_inode = &((struct minix2_inode *) bh->b_data)[(inode->i_ino - 1) % inodes_per_block];

	/* set on disk inode */
	raw_inode->i_mode = inode->i_mode;
	raw_inode->i_uid = inode->i_uid;
	raw_inode->i_size = inode->i_size;
	raw_inode->i_atime = inode->i_atime;
	raw_inode->i_mtime = inode->i_mtime;
	raw_inode->i_ctime = inode->i_ctime;
	raw_inode->i_gid = inode->i_gid;
	raw_inode->i_nlinks = inode->i_nlinks;
	if (S_ISCHR(inode->i_mode) || S_ISBLK(inode->i_mode))
		raw_inode->i_zone[0] = inode->i_rdev;
	else
		for (i = 0; i < 10; i++)
			raw_inode->i_zone[i] = inode->u.minix_i.i_zone[i];

	/* write inode block */
	mark_buffer_dirty(bh);
	brelse(bh);

	return 0;
}

/*
 * Put an inode.
 */
int minix_put_inode(struct inode *inode)
{
	/* check inode */
	if (!inode)
		return -EINVAL;

	/* truncate and free inode */
	if (!inode->i_count && !inode->i_nlinks) {
		inode->i_size = 0;
		minix_truncate(inode);
		minix_free_inode(inode);
	}

	return 0;
}

/*
 * Get an inode buffer.
 */
static int minix_inode_getblk(struct inode *inode, int inode_block, struct buffer_head **bh_res, int create)
{
	struct minix_inode_info *minix_inode = &inode->u.minix_i;
	struct super_block *sb = inode->i_sb;
	int new = 0;

	/* existing block */
	if (minix_inode->i_zone[inode_block])
		goto found;

	/* don't create a new block */
	if (!create)
		return -EIO;

	/* create a new block */
	new = 1;
	minix_inode->i_zone[inode_block] = minix_new_block(inode->i_sb);
	if (!minix_inode->i_zone[inode_block])
		return -EIO;

	/* update inode */
	inode->i_blocks++;
	mark_inode_dirty(inode);

found:
	/* set result */
	if (*bh_res) {
		if (new)
			mark_buffer_new(*bh_res);

		(*bh_res)->b_block = minix_inode->i_zone[inode_block];
		return 0;
	}

	/* new buffer : hash and clear it */
	if (new) {
		*bh_res = getblk(inode->i_dev, minix_inode->i_zone[inode_block], sb->s_blocksize);
		if (!*bh_res)
			return -EIO;

		memset((*bh_res)->b_data, 0, (*bh_res)->b_size);
		mark_buffer_dirty(*bh_res);
		mark_buffer_uptodate(*bh_res, 1);
		return 0;
	}

	/* read block on disk */
	*bh_res = bread(inode->i_dev, minix_inode->i_zone[inode_block], inode->i_sb->s_blocksize);
	if (!*bh_res)
		return -EIO;

	return 0;
}

/*
 * Get a block buffer.
 */
static int minix_block_getblk(struct inode *inode, struct buffer_head *bh, int block_block, struct buffer_head **bh_res, int create)
{
	struct super_block *sb = inode->i_sb;
	int new = 0, i;

	if (!bh)
		return -EIO;

	/* existing block */
	i = ((uint32_t *) bh->b_data)[block_block];
	if (i)
		goto found;

	/* don't create a new block */
	if (!create)
		goto err;

	/* create a new block */
	new = 1;
	i = minix_new_block(inode->i_sb);
	if (!i)
		goto err;

	/* update parent block */
	((uint32_t *) (bh->b_data))[block_block] = i;
	mark_buffer_dirty(bh);

found:
	/* release parent block */
	brelse(bh);

	/* set result */
	if (*bh_res) {
		if (new)
			mark_buffer_new(*bh_res);

		(*bh_res)->b_block = i;
		return 0;
	}

	/* new buffer : hash and clear it */
	if (new) {
		*bh_res = getblk(inode->i_dev, i, sb->s_blocksize);
		if (!*bh_res)
			return -EIO;

		memset((*bh_res)->b_data, 0, (*bh_res)->b_size);
		mark_buffer_dirty(*bh_res);
		mark_buffer_uptodate(*bh_res, 1);
		return 0;
	}

	/* read block on disk */
	*bh_res = bread(inode->i_dev, i, inode->i_sb->s_blocksize);
	if (!*bh_res)
		return -EIO;

	return 0;
err:
	brelse(bh);
	return -EIO;
}

/*
 * Get or create a block.
 */
int minix_get_block(struct inode *inode, uint32_t block, struct buffer_head *bh_res, int create)
{
	struct buffer_head *bh1 = NULL, *bh2 = NULL, *bh3 = NULL;
	struct super_block *sb = inode->i_sb;
	int ret;

	/* check block number */
	if (block >= minix_sb(sb)->s_max_size / sb->s_blocksize)
		return -EIO;

	/* direct block */
	if (block < 7)
		return minix_inode_getblk(inode, block, &bh_res, create);

	/* indirect block */
	block -= 7;
	if (block < 256) {
		ret = minix_inode_getblk(inode, 7, &bh1, create);
		if (ret)
			return ret;
		return minix_block_getblk(inode, bh1, block, &bh_res, create);
	}

	/* double indirect block */
	block -= 256;
	if (block < 256 * 256) {
		ret = minix_inode_getblk(inode, 8, &bh1, create);
		if (ret)
			return ret;
		ret = minix_block_getblk(inode, bh1, (block >> 8) & 255, &bh2, create);
		if (ret)
			return ret;
		return minix_block_getblk(inode, bh2, block & 255, &bh_res, create);
	}

	/* triple indirect block */
	block -= 256 * 256;
	ret = minix_inode_getblk(inode, 9, &bh1, create);
	if (ret)
		return ret;
	ret = minix_block_getblk(inode, bh1, (block >> 16) & 255, &bh2, create);
	if (ret)
		return ret;
	ret = minix_block_getblk(inode, bh2, (block >> 8) & 255, &bh3, create);
	if (ret)
		return ret;
	return minix_block_getblk(inode, bh3, block & 255, &bh_res, create);
}

/*
 * Read a minix inode block.
 */
struct buffer_head *minix_bread(struct inode *inode, int block, int create)
{
	struct super_block *sb = inode->i_sb;
	struct buffer_head tmp = { 0 }, *bh;

	/* get block */
	if (minix_get_block(inode, block, &tmp, create))
		return NULL;

	/* new buffer : hash and clear it */
	if (buffer_new(&tmp)) {
		bh = getblk(inode->i_dev, tmp.b_block, sb->s_blocksize);
		if (!bh)
			return NULL;

		memset(bh->b_data, 0, bh->b_size);
		mark_buffer_dirty(bh);
		mark_buffer_uptodate(bh, 1);
		return bh;
	}

	/* read block on disk */
	return bread(inode->i_dev, tmp.b_block, inode->i_sb->s_blocksize);
}
