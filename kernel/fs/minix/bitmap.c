#include <fs/fs.h>
#include <fs/minix_fs.h>
#include <proc/sched.h>
#include <mm/mm.h>
#include <string.h>
#include <stdio.h>
#include <stderr.h>

#define MINIX_SET_BITMAP(bh, i)			((bh)->b_data[(i) / 8] |= (0x1 << ((i) % 8)))
#define MINIX_CLEAR_BITMAP(bh, i)		((bh)->b_data[(i) / 8] &= ~(0x1 << ((i) % 8)))

/*
 * Count number of free bits in a bitmap.
 */
static uint32_t minix_count_free_bitmap(struct buffer_head **maps, int nb_maps)
{
	uint32_t *bits, res = 0;
	register int i, j, k;

	for (i = 0; i < nb_maps; i++) {
		bits = (uint32_t *) maps[i]->b_data;

		for (j = 0; j < (int) (maps[i]->b_size / 4); j++)
			if (bits[j] != 0xFFFFFFFF)
				for (k = 0; k < 32; k++)
					if (!(bits[j] & (0x1 << k)))
						res++;
	}

	return res;
}

/*
 * Get first free bit in a bitmap block (inode or block).
 */
static inline int minix_get_free_bitmap(struct buffer_head *bh)
{
	uint32_t *bits = (uint32_t *) bh->b_data;
	register int i, j;

	for (i = 0; i < (int) bh->b_size / 4; i++)
		if (bits[i] != 0xFFFFFFFF)
			for (j = 0; j < 32; j++)
				if (!(bits[i] & (0x1 << j)))
					return 32 * i + j;

	return -1;
}

/*
 * Create a new block.
 */
uint32_t minix_new_block(struct super_block *sb)
{
	struct minix_sb_info *sbi = minix_sb(sb);
	struct buffer_head *bh;
	uint32_t block_nr, i;
	int j;

	/* find first free block in bitmap */
	for (i = 0; i < sbi->s_zmap_blocks; i++) {
		j = minix_get_free_bitmap(sbi->s_zmap[i]);
		if (j != -1)
			break;
	}

	/* no free block */
	if (j == -1)
		return 0;

	/* compute real block number */
	block_nr = j + i * sb->s_blocksize * 8 + sbi->s_firstdatazone - 1;
	if (block_nr >= sbi->s_nzones)
		return 0;

	/* get a buffer */
	bh = getblk(sb->s_dev, block_nr, sb->s_blocksize);
	if (!bh)
		return 0;

	/* memzero buffer and release it */
	memset(bh->b_data, 0, bh->b_size);
	bh->b_dirt = 1;
	bh->b_uptodate = 1;
	brelse(bh);

	/* set block in bitmap */
	MINIX_SET_BITMAP(sbi->s_zmap[i], j);
	sbi->s_zmap[i]->b_dirt = 1;
	bwrite(sbi->s_zmap[i]);

	return block_nr;
}

/*
 * Free a block.
 */
int minix_free_block(struct super_block *sb, uint32_t block)
{
	struct minix_sb_info *sbi = minix_sb(sb);
	struct buffer_head *bh;
	uint32_t zone;

	/* check block number */
	if (block < sbi->s_firstdatazone || block >= sbi->s_nzones)
		return -EINVAL;

	/* get buffer and clear it */
	bh = bread(sb->s_dev, block, sb->s_blocksize);
	if (bh) {
		memset(bh->b_data, 0, bh->b_size);
		bh->b_dirt = 1;
		brelse(bh);
	}

	/* update/clear block bitmap */
	zone = block - sbi->s_firstdatazone + 1;
	bh = sbi->s_zmap[zone >> 13];
	MINIX_CLEAR_BITMAP(bh, zone & (bh->b_size * 8 - 1));
	bh->b_dirt = 1;
	bwrite(bh);

	return 0;
}

/*
 * Free an inode.
 */
int minix_free_inode(struct inode *inode)
{
	struct buffer_head *bh;

	if (!inode)
		return 0;

	/* panic if inode is still used */
	if (inode->i_ref > 1) {
		printf("Trying to free inode %d with ref=%d\n", inode->i_ino, inode->i_ref);
		panic("");
	}

	/* update/clear inode bitmap */
	bh = minix_sb(inode->i_sb)->s_imap[inode->i_ino >> 13];
	MINIX_CLEAR_BITMAP(bh, inode->i_ino & (bh->b_size * 8 - 1));
	bh->b_dirt = 1;
	bwrite(bh);

	/* clear inode */
	clear_inode(inode);

	return 0;
}

/*
 * Create a new inode.
 */
struct inode *minix_new_inode(struct super_block *sb)
{
	struct minix_sb_info *sbi = minix_sb(sb);
	struct inode *inode;
	uint32_t i;
	int j;

	/* get an empty new inode */
	inode = get_empty_inode(sb);
	if (!inode)
		return NULL;

	/* find first free inode in bitmap */
	for (i = 0; i < sbi->s_imap_blocks; i++) {
		j = minix_get_free_bitmap(sbi->s_imap[i]);
		if (j != -1)
			break;
	}

	/* no free inode */
	if (j == -1)
		iput(inode);

	/* set inode */
	inode->i_atime = inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	inode->i_nlinks = 1;
	inode->i_ino = i * sb->s_blocksize * 8 + j;
	inode->i_ref = 1;
	inode->i_sb = sb;

	/* set inode in bitmap */
	MINIX_SET_BITMAP(sbi->s_imap[i], j);
	sbi->s_imap[i]->b_dirt = 1;
	bwrite(sbi->s_imap[i]);

	return inode;
}

/*
 * Get number of free inodes.
 */
uint32_t minix_count_free_inodes(struct super_block *sb)
{
	struct minix_sb_info *sbi = minix_sb(sb);

	return minix_count_free_bitmap(sbi->s_imap, sbi->s_imap_blocks);
}

/*
 * Get number of free blocks.
 */
uint32_t minix_count_free_blocks(struct super_block *sb)
{
	struct minix_sb_info *sbi = minix_sb(sb);

	return minix_count_free_bitmap(sbi->s_zmap, sbi->s_zmap_blocks);
}
