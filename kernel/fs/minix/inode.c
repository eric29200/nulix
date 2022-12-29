#include <fs/fs.h>
#include <fs/minix_fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <drivers/tty.h>
#include <stdio.h>
#include <stderr.h>
#include <fcntl.h>
#include <string.h>

/*
 * Directory operations.
 */
struct file_operations_t minix_dir_fops = {
	.read			= minix_file_read,
	.write			= minix_file_write,
	.getdents64		= minix_getdents64,
};

/*
 * File operations.
 */
struct file_operations_t minix_file_fops = {
	.read			= minix_file_read,
	.write			= minix_file_write,
	.mmap			= generic_file_mmap,
};

/*
 * Minix symbolic link inode operations.
 */
struct inode_operations_t minix_symlink_iops = {
	.follow_link		= minix_follow_link,
	.readlink		= minix_readlink,
};

/*
 * Minix file inode operations.
 */
struct inode_operations_t minix_file_iops = {
	.fops			= &minix_file_fops,
	.truncate		= minix_truncate,
	.bmap			= minix_bmap,
	.readpage		= generic_readpage,
};

/*
 * Minix directory inode operations.
 */
struct inode_operations_t minix_dir_iops = {
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
int minix_read_inode(struct inode_t *inode)
{
	struct minix_sb_info_t *sbi = minix_sb(inode->i_sb);
	struct minix2_inode_t *raw_inode;
	struct buffer_head_t *bh;
	int inodes_per_block, i;
	uint32_t block;

	/* check inode number */
	if (!inode->i_ino || inode->i_ino > sbi->s_ninodes)
		return -EINVAL;

	/* compute inode store block */
	inodes_per_block = inode->i_sb->s_blocksize / sizeof(struct minix2_inode_t);
	block = 2 + sbi->s_imap_blocks + sbi->s_zmap_blocks + (inode->i_ino - 1) / inodes_per_block;

	/* read inode store block */
	bh = bread(inode->i_sb->s_dev, block, inode->i_sb->s_blocksize);
	if (!bh) {
		iput(inode);
		return -EIO;
	}

	/* get raw inode */
	raw_inode = &((struct minix2_inode_t *) bh->b_data)[(inode->i_ino - 1) % inodes_per_block];

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
		inode->i_op = char_get_driver(inode);
	} else if (S_ISBLK(inode->i_mode)) {
		inode->i_rdev = inode->u.minix_i.i_zone[0];
		inode->i_op = block_get_driver(inode);
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
int minix_write_inode(struct inode_t *inode)
{
	struct minix_sb_info_t *sbi = minix_sb(inode->i_sb);
	struct minix2_inode_t *raw_inode;
	struct buffer_head_t *bh;
	int inodes_per_block, i;
	uint32_t block;

	/* compute inode store block */
	inodes_per_block = inode->i_sb->s_blocksize / sizeof(struct minix2_inode_t);
	block = 2 + sbi->s_imap_blocks + sbi->s_zmap_blocks + (inode->i_ino - 1) / inodes_per_block;

	/* read inode store block */
	bh = bread(inode->i_sb->s_dev, block, inode->i_sb->s_blocksize);
	if (!bh) {
		iput(inode);
		return -EIO;
	}

	/* get raw inode */
	raw_inode = &((struct minix2_inode_t *) bh->b_data)[(inode->i_ino - 1) % inodes_per_block];

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
	bh->b_dirt = 1;
	brelse(bh);

	return 0;
}

/*
 * Put an inode.
 */
int minix_put_inode(struct inode_t *inode)
{
	/* check inode */
	if (!inode)
		return -EINVAL;

	/* truncate and free inode */
	if (!inode->i_ref && !inode->i_nlinks) {
		inode->i_size = 0;
		minix_truncate(inode);
		minix_free_inode(inode);
	}

	return 0;
}

/*
 * Get an inode buffer.
 */
static struct buffer_head_t *inode_getblk(struct inode_t *inode, int nr, int create)
{
	/* create block if needed */
	if (create && !inode->u.minix_i.i_zone[nr])
		if ((inode->u.minix_i.i_zone[nr] = minix_new_block(inode->i_sb)))
			inode->i_dirt = 1;

	if (!inode->u.minix_i.i_zone[nr])
		return NULL;

	/* read block from device */
	return bread(inode->i_sb->s_dev, inode->u.minix_i.i_zone[nr], inode->i_sb->s_blocksize);
}

/*
 * Get a block buffer.
 */
static struct buffer_head_t *block_getblk(struct inode_t *inode, struct buffer_head_t *bh, int block, int create)
{
	int i;

	if (!bh)
		return NULL;

	/* create block if needed */
	i = ((uint32_t *) bh->b_data)[block];
	if (create && !i) {
		if ((i = minix_new_block(inode->i_sb))) {
			((uint32_t *) (bh->b_data))[block] = i;
			bh->b_dirt = 1;
		}
	}

	/* release block */
	brelse(bh);

	if (!i)
		return NULL;

	/* read block from device */
	return bread(inode->i_sb->s_dev, i, inode->i_sb->s_blocksize);
}

/*
 * Get or create a buffer.
 */
struct buffer_head_t *minix_getblk(struct inode_t *inode, int block, int create)
{
	struct super_block_t *sb = inode->i_sb;
	struct buffer_head_t *bh;

	/* check block number */
	if (block < 0 || (uint32_t) block >= minix_sb(sb)->s_max_size / sb->s_blocksize)
		return NULL;

	/* direct block */
	if (block < 7)
		return inode_getblk(inode, block, create);

	/* indirect block */
	block -= 7;
	if (block < 256) {
		bh = inode_getblk(inode, 7, create);
		return block_getblk(inode, bh, block, create);
	}

	/* double indirect block */
	block -= 256;
	if (block < 256 * 256) {
		bh = inode_getblk(inode, 8, create);
		bh = block_getblk(inode, bh, (block >> 8) & 255, create);
		return block_getblk(inode, bh, block & 255, create);
	}

	/* triple indirect block */
	block -= 256 * 256;
	bh = inode_getblk(inode, 9, create);
	bh = block_getblk(inode, bh, (block >> 16) & 255, create);
	bh = block_getblk(inode, bh, (block >> 8) & 255, create);
	return block_getblk(inode, bh, block & 255, create);
}

/*
 * Get a block number.
 */
static int inode_bmap(struct inode_t *inode, int nr)
{
	return inode->u.minix_i.i_zone[nr];
}

/*
 * Get a block number.
 */
static int block_bmap(struct buffer_head_t *bh, int nr)
{
	int ret;

	if (!bh)
		return 0;

	ret = ((uint32_t *) bh->b_data)[nr];
	brelse(bh);
	return ret;
}

/*
 * Get a block number.
 */
int minix_bmap(struct inode_t *inode, int block)
{
	struct super_block_t *sb = inode->i_sb;
	int i;

	/* check block number */
	if (block < 0 || (uint32_t) block >= minix_sb(sb)->s_max_size / sb->s_blocksize)
		return 0;

	/* direct block */
	if (block < 7)
		return inode_bmap(inode, block);

	/* indirect block */
	block -= 7;
	if (block < 256) {
		i = inode_bmap(inode, 7);
		if (!i)
			return 0;
		return block_bmap(bread(sb->s_dev, i, sb->s_blocksize), block);
	}

	/* double indirect block */
	block -= 256;
	if (block < 256 * 256) {
		i = inode_bmap(inode, 8);
		if (!i)
			return 0;
		i = block_bmap(bread(sb->s_dev, i, sb->s_blocksize), (block >> 8) & 255);
		if (!i)
			return 0;
		return block_bmap(bread(sb->s_dev, i, sb->s_blocksize), block & 255);
	}

	/* triple indirect block */
	block -= 256 * 256;
	i = inode_bmap(inode, 9);
	if (!i)
		return 0;
	i = block_bmap(bread(sb->s_dev, i, sb->s_blocksize), (block >> 16) & 255);
	if (!i)
		return 0;
	i = block_bmap(bread(sb->s_dev, i, sb->s_blocksize), (block >> 8) & 255);
	if (!i)
		return 0;
	return block_bmap(bread(sb->s_dev, i, sb->s_blocksize), block & 255);
}
