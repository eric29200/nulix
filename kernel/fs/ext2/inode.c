#include <fs/fs.h>
#include <fs/ext2_fs.h>
#include <stderr.h>
#include <stdio.h>
#include <fcntl.h>

/*
 * Ext2 file operations.
 */
struct file_operations ext2_file_fops = {
	.read		= generic_file_read,
	.write		= generic_file_write,
	.mmap		= generic_file_mmap,
};

/*
 * Ext2 directory operations.
 */
struct file_operations ext2_dir_fops = {
	.getdents64	= ext2_getdents64,
};

/*
 * Ext2 file inode operations.
 */
struct inode_operations ext2_file_iops = {
	.fops		= &ext2_file_fops,
	.truncate	= ext2_truncate,
	.get_block	= ext2_get_block,
	.bmap		= generic_block_bmap,
	.readpage	= generic_readpage,
	.prepare_write	= generic_prepare_write,
	.commit_write	= generic_commit_write,
};

/*
 * Ext2 fast symbolic link inode operations.
 */
struct inode_operations ext2_fast_symlink_iops = {
	.follow_link	= ext2_fast_follow_link,
	.readlink	= ext2_fast_readlink,
};

/*
 * Ext2 page symbolic link inode operations.
 */
struct inode_operations ext2_page_symlink_iops = {
	.follow_link	= ext2_page_follow_link,
	.readlink	= ext2_page_readlink,
};

/*
 * Ext2 directory inode operations.
 */
struct inode_operations ext2_dir_iops = {
	.fops		= &ext2_dir_fops,
	.lookup		= ext2_lookup,
	.create		= ext2_create,
	.link		= ext2_link,
	.unlink		= ext2_unlink,
	.symlink	= ext2_symlink,
	.mkdir		= ext2_mkdir,
	.rmdir		= ext2_rmdir,
	.rename		= ext2_rename,
	.mknod		= ext2_mknod,
	.truncate	= ext2_truncate,
};

/*
 * Test whether an inode is a fast symlink.
 */
static inline int ext2_inode_is_fast_symlink(struct inode *inode)
{
	int ea_blocks = inode->u.ext2_i.i_file_acl ? (inode->i_sb->s_blocksize >> 9) : 0;
	return S_ISLNK(inode->i_mode) && (inode->i_blocks - ea_blocks == 0);
}

/*
 * Read an inode on disk.
 */
int ext2_read_inode(struct inode *inode)
{
	struct ext2_inode_info *ext2_inode = &inode->u.ext2_i;
	struct ext2_sb_info *sbi = ext2_sb(inode->i_sb);
	uint32_t block_group, offset, block;
	struct ext2_group_desc *gdp;
	struct ext2_inode *raw_inode;
	struct buffer_head *bh;
	int i;

	/* check inode number */
	if ((inode->i_ino != EXT2_ROOT_INO && inode->i_ino < sbi->s_first_ino) || inode->i_ino > sbi->s_es->s_inodes_count)
		return -EINVAL;

	/* get group descriptor */
	block_group = (inode->i_ino - 1) / sbi->s_inodes_per_group;
	gdp = ext2_get_group_desc(inode->i_sb, block_group, NULL);
	if (!gdp)
		return -EINVAL;

	/* get inode table block */
	offset = ((inode->i_ino - 1) % sbi->s_inodes_per_group) * sbi->s_inode_size;
	block = gdp->bg_inode_table + (offset >> inode->i_sb->s_blocksize_bits);

	/* read inode table block buffer */
	bh = bread(inode->i_sb->s_dev, block, inode->i_sb->s_blocksize);
	if (!bh)
		return -EIO;

	/* get inode */
	offset &= (inode->i_sb->s_blocksize - 1);
	raw_inode = (struct ext2_inode *) (bh->b_data + offset);

	/* set generic inode */
	inode->i_mode = raw_inode->i_mode;
	inode->i_nlinks = raw_inode->i_links_count;
	inode->i_uid = raw_inode->i_uid | (raw_inode->i_uid_high << 16);
	inode->i_gid = raw_inode->i_gid | (raw_inode->i_gid_high << 16);
	inode->i_size = raw_inode->i_size;
	inode->i_blocks = raw_inode->i_blocks;
	inode->i_atime = raw_inode->i_atime;
	inode->i_mtime = raw_inode->i_mtime;
	inode->i_ctime = raw_inode->i_ctime;
	ext2_inode->i_flags = raw_inode->i_flags;
	ext2_inode->i_faddr = raw_inode->i_faddr;
	ext2_inode->i_frag_no = raw_inode->i_frag;
	ext2_inode->i_frag_size = raw_inode->i_fsize;
	ext2_inode->i_file_acl = raw_inode->i_file_acl;
	ext2_inode->i_dir_acl = raw_inode->i_dir_acl;
	ext2_inode->i_dtime = raw_inode->i_dtime;
	ext2_inode->i_generation = raw_inode->i_generation;
	ext2_inode->i_block_group = block_group;
	for (i = 0; i < EXT2_N_BLOCKS; i++)
		ext2_inode->i_data[i] = raw_inode->i_block[i];

	/* set operations */
	if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &ext2_dir_iops;
	} else if (S_ISLNK(inode->i_mode)) {
		if (ext2_inode_is_fast_symlink(inode))
			inode->i_op = &ext2_fast_symlink_iops;
		else
			inode->i_op = &ext2_page_symlink_iops;
	} else if (S_ISCHR(inode->i_mode)) {
		inode->i_rdev = raw_inode->i_block[0];
		inode->i_op = char_get_driver(inode);
	} else if (S_ISBLK(inode->i_mode)) {
		inode->i_rdev = raw_inode->i_block[0];
		inode->i_op = block_get_driver(inode);
	} else {
		inode->i_op = &ext2_file_iops;
	}

	/* release block buffer */
	brelse(bh);

	return 0;
}

/*
 * Write an inode on disk.
 */
int ext2_write_inode(struct inode *inode)
{
	struct ext2_inode_info *ext2_inode = &inode->u.ext2_i;
	struct ext2_sb_info *sbi = ext2_sb(inode->i_sb);
	uint32_t block_group, offset, block;
	struct ext2_inode *raw_inode;
	struct ext2_group_desc *gdp;
	struct buffer_head *bh;
	int i;

	/* check inode number */
	if ((inode->i_ino != EXT2_ROOT_INO && inode->i_ino < sbi->s_first_ino) || inode->i_ino > sbi->s_es->s_inodes_count)
		return -EINVAL;

	/* get group descriptor */
	block_group = (inode->i_ino - 1) / sbi->s_inodes_per_group;
	gdp = ext2_get_group_desc(inode->i_sb, block_group, NULL);
	if (!gdp)
		return -EINVAL;

	/* get inode table block */
	offset = ((inode->i_ino - 1) % sbi->s_inodes_per_group) * sbi->s_inode_size;
	block = gdp->bg_inode_table + (offset >> inode->i_sb->s_blocksize_bits);

	/* read inode table block buffer */
	bh = bread(inode->i_sb->s_dev, block, inode->i_sb->s_blocksize);
	if (!bh)
		return -EIO;

	/* get inode */
	offset &= (inode->i_sb->s_blocksize - 1);
	raw_inode = (struct ext2_inode *) (bh->b_data + offset);

	/* set raw inode */

	/* set generic inode */
	raw_inode->i_mode = inode->i_mode;
	raw_inode->i_uid = inode->i_uid & 0xFFFF;
	raw_inode->i_size = inode->i_size;
	raw_inode->i_atime = inode->i_atime;
	raw_inode->i_ctime = inode->i_ctime;
	raw_inode->i_mtime = inode->i_mtime;
	raw_inode->i_dtime = ext2_inode->i_dtime;
	raw_inode->i_gid = inode->i_gid & 0xFFFF;
	raw_inode->i_links_count = inode->i_nlinks;
	raw_inode->i_blocks = inode->i_blocks;
	raw_inode->i_flags = ext2_inode->i_flags;
	raw_inode->i_generation = ext2_inode->i_generation;
	raw_inode->i_file_acl = ext2_inode->i_file_acl;
	raw_inode->i_dir_acl = ext2_inode->i_dir_acl;
	raw_inode->i_faddr = ext2_inode->i_faddr;
	raw_inode->i_frag = ext2_inode->i_frag_no;
	raw_inode->i_fsize = ext2_inode->i_frag_size;
	raw_inode->i_uid_high = (inode->i_uid & 0xFFFF0000) >> 16;
	raw_inode->i_gid_high = (inode->i_gid & 0xFFFF0000) >> 16;
	for (i = 0; i < EXT2_N_BLOCKS; i++)
		raw_inode->i_block[i] = ext2_inode->i_data[i];

	/* release block buffer */
	mark_buffer_dirty(bh);
	brelse(bh);

	return 0;
}

/*
 * Put an inode.
 */
int ext2_put_inode(struct inode *inode)
{
	/* check inode */
	if (!inode)
		return -EINVAL;

	/* truncate and free inode */
	if (!inode->i_count && !inode->i_nlinks) {
		inode->i_size = 0;
		ext2_truncate(inode);
		ext2_free_inode(inode);
	}

	return 0;
}

/*
 * Read a Ext2 inode block.
 */
static int ext2_inode_getblk(struct inode *inode, int inode_block, struct buffer_head **bh_res, int create)
{
	struct ext2_inode_info *ext2_inode = &inode->u.ext2_i;
	struct ext2_sb_info *sbi = ext2_sb(inode->i_sb);
	struct super_block *sb = inode->i_sb;
	uint32_t goal = 0;
	int new = 0, i;

	/* existing block */
	if (ext2_inode->i_data[inode_block])
		goto found;

	/* don't create a new block */
	if (!create)
		return -EIO;

	/* try to reuse last block of inode */
	for (i = inode_block - 1; i >= 0; i--) {
		if (ext2_inode->i_data[i]) {
			goal = ext2_inode->i_data[i];
			break;
		}
	}

	/* else use block of this inode */
	if (!goal)
		goal = ext2_inode->i_block_group * sbi->s_blocks_per_group + sbi->s_es->s_first_data_block;

	/* create new block */
	new = 1;
	ext2_inode->i_data[inode_block] = ext2_new_block(inode, goal);
	if (!ext2_inode->i_data[inode_block])
		return -EIO;

	/* update inode */
	inode->i_blocks++;
	mark_inode_dirty(inode);

found:
	/* set result */
	if (*bh_res) {
		if (new)
			mark_buffer_new(*bh_res);

		(*bh_res)->b_block = ext2_inode->i_data[inode_block];
		return 0;
	}

	/* new buffer : hash and clear it */
	if (new) {
		*bh_res = getblk(sb->s_dev, ext2_inode->i_data[inode_block], sb->s_blocksize);
		if (!*bh_res)
			return -EIO;

		memset((*bh_res)->b_data, 0, (*bh_res)->b_size);
		mark_buffer_dirty(*bh_res);
		mark_buffer_uptodate(*bh_res, 1);
		return 0;
	}

	/* read block on disk */
	*bh_res = bread(inode->i_sb->s_dev, ext2_inode->i_data[inode_block], inode->i_sb->s_blocksize);
	if (!*bh_res)
		return -EIO;

	return 0;
}

/*
 * Read a Ext2 indirect block.
 */
static int ext2_block_getblk(struct inode *inode, struct buffer_head *bh, int block_block, struct buffer_head **bh_res, int create)
{
	struct super_block *sb = inode->i_sb;
	int i, tmp, new = 0;
	uint32_t goal = 0;

	if (!bh)
		return -EIO;

	/* existing block */
	i = ((uint32_t *) bh->b_data)[block_block];
	if (i)
		goto found;

	/* don't create a new block */
	if (!create)
		goto err;

	/* try to reuse previous blocks */
	for (tmp = block_block - 1; tmp >= 0; tmp--) {
		if (((uint32_t *) bh->b_data)[tmp]) {
			goal = ((uint32_t *) bh->b_data)[tmp];
		}
	}

	/* else use this indirect block */
	if (!goal)
		goal = bh->b_block;

	/* create new block */
	new = 1;
	i = ext2_new_block(inode, goal);
	if (!i)
		goto err;

	/* update parent block */
	((uint32_t *) bh->b_data)[block_block] = i;
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
		*bh_res = getblk(sb->s_dev, i, sb->s_blocksize);
		if (!*bh_res)
			return -EIO;

		memset((*bh_res)->b_data, 0, (*bh_res)->b_size);
		mark_buffer_dirty(*bh_res);
		mark_buffer_uptodate(*bh_res, 1);
		return 0;
	}

	/* read block on disk */
	*bh_res = bread(inode->i_sb->s_dev, i, inode->i_sb->s_blocksize);
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
int ext2_get_block(struct inode *inode, uint32_t block, struct buffer_head *bh_res, int create)
{
	struct buffer_head *bh1 = NULL, *bh2 = NULL, *bh3 = NULL;
	struct super_block *sb = inode->i_sb;
	uint32_t addr_per_block;
	int ret;

	/* compute number of addresses per block */
	addr_per_block = sb->s_blocksize / 4;

	/* check block number */
	if (block > EXT2_NDIR_BLOCKS + addr_per_block
			+ addr_per_block * addr_per_block
			+ addr_per_block * addr_per_block * addr_per_block)
		return -EIO;

	/* direct block */
	if (block < EXT2_NDIR_BLOCKS)
		return ext2_inode_getblk(inode, block, &bh_res, create);

	/* indirect block */
	block -= EXT2_NDIR_BLOCKS;
	if (block < addr_per_block) {
		ret = ext2_inode_getblk(inode, EXT2_IND_BLOCK, &bh1, create);
		if (ret)
			return ret;
		return ext2_block_getblk(inode, bh1, block, &bh_res, create);
	}

	/* double indirect block */
	block -= addr_per_block;
	if (block < addr_per_block * addr_per_block) {
		ret = ext2_inode_getblk(inode, EXT2_DIND_BLOCK, &bh1, create);
		if (ret)
			return ret;
		ret = ext2_block_getblk(inode, bh1, block / addr_per_block, &bh2, create);
		if (ret)
			return ret;
		return ext2_block_getblk(inode, bh2, block & (addr_per_block - 1), &bh_res, create);
	}

	/* triple indirect block */
	block -= addr_per_block * addr_per_block;
	ret = ext2_inode_getblk(inode, EXT2_TIND_BLOCK, &bh1, create);
	if (ret)
		return ret;
	ret = ext2_block_getblk(inode, bh1, block / (addr_per_block * addr_per_block), &bh2, create);
	if (ret)
		return ret;
	ret = ext2_block_getblk(inode, bh2, (block / addr_per_block) & (addr_per_block - 1), &bh3, create);
	if (ret)
		return ret;
	return ext2_block_getblk(inode, bh3, block & (addr_per_block - 1), &bh_res, create);
}

/*
 * Read a Ext2 inode block.
 */
struct buffer_head *ext2_bread(struct inode *inode, uint32_t block, int create)
{
	struct super_block *sb = inode->i_sb;
	struct buffer_head tmp = { 0 }, *bh;

	/* get block */
	if (ext2_get_block(inode, block, &tmp, create))
		return NULL;

	/* new buffer : hash and clear it */
	if (buffer_new(&tmp)) {
		bh = getblk(sb->s_dev, tmp.b_block, sb->s_blocksize);
		if (!bh)
			return NULL;

		memset(bh->b_data, 0, bh->b_size);
		mark_buffer_dirty(bh);
		mark_buffer_uptodate(bh, 1);
		return bh;
	}

	/* read block on disk */
	return bread(inode->i_sb->s_dev, tmp.b_block, inode->i_sb->s_blocksize);
}
