#include <fs/fs.h>
#include <fs/ext2_fs.h>
#include <stderr.h>
#include <stdio.h>
#include <string.h>

/*
 * Get a group descriptor.
 */
struct ext2_group_desc *ext2_get_group_desc(struct super_block *sb, uint32_t block_group, struct buffer_head **bh)
{
	struct ext2_sb_info *sbi = ext2_sb(sb);
	struct ext2_group_desc *desc;
	uint32_t group_desc, offset;

	/* check block group */
	if (block_group >= sbi->s_groups_count)
		return NULL;

	/* compute group descriptor block */
	group_desc = block_group / sbi->s_desc_per_block;
	offset = block_group % sbi->s_desc_per_block;
	if (!sbi->s_group_desc[group_desc])
		return NULL;

	/* group block buffer of group descriptor */
	desc = (struct ext2_group_desc *) sbi->s_group_desc[group_desc]->b_data;
	if (bh)
		*bh = sbi->s_group_desc[group_desc];

	return desc + offset;
}

/*
 * Read the blocks bitmap of a block group.
 */
static struct buffer_head *ext2_read_block_bitmap(struct super_block *sb, uint32_t block_group)
{
	struct ext2_group_desc *gdp;

	/* get group descriptor */
	gdp = ext2_get_group_desc(sb, block_group, NULL);
	if (!gdp)
		return NULL;

	/* load block bitmap */
	return bread(sb->s_dev, gdp->bg_block_bitmap, sb->s_blocksize);
}

/*
 * Create a new Ext2 block (try goal block first).
 */
int ext2_new_block(struct inode *inode, uint32_t goal)
{
	struct ext2_sb_info *sbi = ext2_sb(inode->i_sb);
	struct buffer_head *gdp_bh, *bitmap_bh;
	struct ext2_group_desc *gdp;
	uint32_t group_no, bgi;
	int grp_alloc_block;

	/* adjust goal block */
	if (goal < sbi->s_es->s_first_data_block || goal >= sbi->s_es->s_blocks_count)
		goal = sbi->s_es->s_first_data_block;

	/* try to find a group with free blocks (start with goal group) */
	group_no = (goal - sbi->s_es->s_first_data_block) / sbi->s_blocks_per_group;
	for (bgi = 0; bgi < sbi->s_groups_count; bgi++, group_no++) {
		/* rewind to first group if needed */
		if (group_no >= sbi->s_groups_count)
			group_no = 0;

		/* get group descriptor */
		gdp = ext2_get_group_desc(inode->i_sb, group_no, &gdp_bh);
		if (!gdp)
			return -EIO;

		/* no free blocks in this group */
		if (!gdp->bg_free_blocks_count)
			continue;

		/* get group blocks bitmap */
		bitmap_bh = ext2_read_block_bitmap(inode->i_sb, group_no);
		if (!bitmap_bh)
			return -EIO;

		/* get first free block from bitmap */
		grp_alloc_block = ext2_get_free_bitmap(bitmap_bh);
		if (grp_alloc_block != -1 && grp_alloc_block < (int) sbi->s_blocks_per_group)
			goto allocated;

		/* release bitmap block */
		brelse(bitmap_bh);
	}

	return 0;
allocated:
	/* set block in bitmap */
	EXT2_BITMAP_SET(bitmap_bh, grp_alloc_block);

	/* release block bitmap */
	mark_buffer_dirty(bitmap_bh);
	brelse(bitmap_bh);

	/* update group descriptor */
	gdp->bg_free_blocks_count = gdp->bg_free_blocks_count - 1;
	mark_buffer_dirty(gdp_bh);

	/* update super block */
	sbi->s_es->s_free_blocks_count = sbi->s_es->s_free_blocks_count - 1;
	mark_buffer_dirty(sbi->s_sbh);

	/* mark inode dirty */
	inode->i_dirt = 1;

	/* compute global position of block */
	return grp_alloc_block + ext2_group_first_block_no(inode->i_sb, group_no);
}

/*
 * Free a Ext2 block.
 */
int ext2_free_block(struct inode *inode, uint32_t block)
{
	struct ext2_sb_info *sbi = ext2_sb(inode->i_sb);
	struct buffer_head *bitmap_bh, *gdp_bh, *bh;
	struct ext2_group_desc *gdp;
	uint32_t block_group, bit;

	/* check block number */
	if (block < sbi->s_es->s_first_data_block || block >= sbi->s_es->s_blocks_count) {
		printf("[Ext2-fs] Trying to free block %d not in data zone\n", block);
		return -EINVAL;
	}

	/* clear block buffer */
	bh = find_buffer(inode->i_sb->s_dev, block, inode->i_sb->s_blocksize);
	if (bh) {
		mark_buffer_clean(bh);
		brelse(bh);
	}

	/* get block group */
	block_group = (block - sbi->s_es->s_first_data_block) / sbi->s_blocks_per_group;
	bit = (block - sbi->s_es->s_first_data_block) % sbi->s_blocks_per_group;

	/* get block bitmap */
	bitmap_bh = ext2_read_block_bitmap(inode->i_sb, block_group);
	if (!bitmap_bh)
		return -EIO;

	/* clear block in bitmap */
	EXT2_BITMAP_CLR(bitmap_bh, bit);
	mark_buffer_dirty(bitmap_bh);
	brelse(bitmap_bh);

	/* update group descriptor */
	gdp = ext2_get_group_desc(inode->i_sb, block_group, &gdp_bh);
	gdp->bg_free_blocks_count = gdp->bg_free_blocks_count + 1;
	mark_buffer_dirty(gdp_bh);

	/* update super block */
	sbi->s_es->s_free_blocks_count = sbi->s_es->s_free_blocks_count + 1;
	mark_buffer_dirty(sbi->s_sbh);

	return 0;
}
