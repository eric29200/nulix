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
};

/*
 * Minix file inode operations.
 */
struct inode_operations_t minix_file_iops = {
	.fops			= &minix_file_fops,
	.follow_link		= minix_follow_link,
	.readlink		= minix_readlink,
	.truncate		= minix_truncate,
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
 * Read a Minix V1 inode on disk.
 */
static int minix_read_inode_v1(struct inode_t *inode)
{
	struct minix_sb_info_t *sbi = minix_sb(inode->i_sb);
	struct minix1_inode_t *raw_inode;
	struct buffer_head_t *bh;
	int inodes_per_block, i;
	uint32_t block;

	/* compute inode store block */
	inodes_per_block = inode->i_sb->s_blocksize / sizeof(struct minix1_inode_t);
	block = 2 + sbi->s_imap_blocks + sbi->s_zmap_blocks + (inode->i_ino - 1) / inodes_per_block;

	/* read inode store block */
	bh = bread(inode->i_sb, block);
	if (!bh) {
		iput(inode);
		return -EIO;
	}

	/* get raw inode */
	raw_inode = &((struct minix1_inode_t *) bh->b_data)[(inode->i_ino - 1) % inodes_per_block];

	/* set inode */
	inode->i_mode = raw_inode->i_mode;
	inode->i_nlinks = raw_inode->i_nlinks;
	inode->i_uid = raw_inode->i_uid;
	inode->i_gid = raw_inode->i_gid;
	inode->i_size = raw_inode->i_size;
	inode->i_atime = raw_inode->i_time;
	inode->i_mtime = raw_inode->i_time;
	inode->i_ctime = raw_inode->i_time;
	inode->i_blocks = 0;
	for (i = 0; i < 10; i++)
		inode->u.minix_i.i_zone[i] = raw_inode->i_zone[i];

	if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &minix_dir_iops;
	} else if (S_ISCHR(inode->i_mode)) {
		inode->i_rdev = inode->u.minix_i.i_zone[0];
		inode->i_op = char_get_driver(inode);
	} else if (S_ISBLK(inode->i_mode)) {
		inode->i_rdev = inode->u.minix_i.i_zone[0];
		inode->i_op = NULL;
	} else {
		inode->i_op = &minix_file_iops;
	}

	/* release block buffer */
	brelse(bh);

	return 0;
}

/*
 * Read a Minix V2 inode on disk.
 */
static int minix_read_inode_v2(struct inode_t *inode)
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
	bh = bread(inode->i_sb, block);
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

	if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &minix_dir_iops;
	} else if (S_ISCHR(inode->i_mode)) {
		inode->i_rdev = inode->u.minix_i.i_zone[0];
		inode->i_op = char_get_driver(inode);
	} else if (S_ISBLK(inode->i_mode)) {
		inode->i_rdev = inode->u.minix_i.i_zone[0];
		inode->i_op = NULL;
	} else {
		inode->i_op = &minix_file_iops;
	}

	/* release block buffer */
	brelse(bh);

	return 0;
}

/*
 * Read a Minix inode on disk.
 */
int minix_read_inode(struct inode_t *inode)
{
	struct minix_sb_info_t *sbi = minix_sb(inode->i_sb);
	int err;

	/* check inode number */
	if (!inode->i_ino || inode->i_ino > sbi->s_ninodes)
		return -EINVAL;

	/* read inode on disk */
	if (sbi->s_version == MINIX_V1)
		err = minix_read_inode_v1(inode);
	else
		err = minix_read_inode_v2(inode);

	/* set operations */
	if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &minix_dir_iops;
	} else if (S_ISCHR(inode->i_mode)) {
		inode->i_rdev = inode->u.minix_i.i_zone[0];
		inode->i_op = char_get_driver(inode);
	} else if (S_ISBLK(inode->i_mode)) {
		inode->i_rdev = inode->u.minix_i.i_zone[0];
		inode->i_op = NULL;
	} else {
		inode->i_op = &minix_file_iops;
	}

	return err;
}

/*
 * Write a Minix V1 inode on disk.
 */
static int minix_write_inode_v1(struct inode_t *inode)
{

	struct minix_sb_info_t *sbi = minix_sb(inode->i_sb);
	struct minix1_inode_t *raw_inode;
	struct buffer_head_t *bh;
	int inodes_per_block, i;
	uint32_t block;

	/* compute inode store block */
	inodes_per_block = inode->i_sb->s_blocksize / sizeof(struct minix1_inode_t);
	block = 2 + sbi->s_imap_blocks + sbi->s_zmap_blocks + (inode->i_ino - 1) / inodes_per_block;

	/* read inode store block */
	bh = bread(inode->i_sb, block);
	if (!bh) {
		iput(inode);
		return -EIO;
	}

	/* get raw inode */
	raw_inode = &((struct minix1_inode_t *) bh->b_data)[(inode->i_ino - 1) % inodes_per_block];

	/* set on disk inode */
	raw_inode->i_mode = inode->i_mode;
	raw_inode->i_uid = inode->i_uid;
	raw_inode->i_size = inode->i_size;
	raw_inode->i_time = inode->i_mtime;
	raw_inode->i_gid = inode->i_gid;
	raw_inode->i_nlinks = inode->i_nlinks;
	for (i = 0; i < 10; i++)
		raw_inode->i_zone[i] = inode->u.minix_i.i_zone[i];

	/* write inode block */
	bh->b_dirt = 1;
	brelse(bh);

	return 0;
}

/*
 * Write a Minix V2/V3 inode on disk.
 */
static int minix_write_inode_v2(struct inode_t *inode)
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
	bh = bread(inode->i_sb, block);
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
	for (i = 0; i < 10; i++)
		raw_inode->i_zone[i] = inode->u.minix_i.i_zone[i];

	/* write inode block */
	bh->b_dirt = 1;
	brelse(bh);

	return 0;
}

/*
 * Write a Minix inode on disk.
 */
int minix_write_inode(struct inode_t *inode)
{
	struct minix_sb_info_t *sbi = minix_sb(inode->i_sb);

	if (sbi->s_version == MINIX_V1)
		return minix_write_inode_v1(inode);
	else
		return minix_write_inode_v2(inode);
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
	if (!inode->i_nlinks) {
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
	return bread(inode->i_sb, inode->u.minix_i.i_zone[nr]);
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
	return bread(inode->i_sb, i);
}

/*
 * Read a buffer.
 */
struct buffer_head_t *minix_bread(struct inode_t *inode, int block, int create)
{
	struct buffer_head_t *bh;

	/* check block number */
	if (block < 0 || (uint32_t) block >= minix_sb(inode->i_sb)->s_max_size / MINIX_BLOCK_SIZE)
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
