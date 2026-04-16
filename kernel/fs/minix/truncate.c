#include <fs/fs.h>
#include <fs/minix_fs.h>
#include <fcntl.h>
#include <stdio.h>

#define DIRECT_BLOCK(inode)				(((int) (inode)->i_size + 1023) >> 10)
#define INDIRECT_BLOCK(inode, offset)			(DIRECT_BLOCK(inode) - offset)
#define V1_DINDIRECT_BLOCK(inode, offset)		((DIRECT_BLOCK(inode) - offset) >> 9)
#define V2_DINDIRECT_BLOCK(inode, offset)		((DIRECT_BLOCK(inode) - offset) >> 8)
#define TINDIRECT_BLOCK(inode, offset)			((DIRECT_BLOCK(inode) - offset) >> 8)

/*
 * Free direct blocks.
 */
static void V1_minix_free_direct_block(struct inode *inode)
{
	int i;

	for (i = DIRECT_BLOCK(inode); i < 7; i++) {
		if (inode->u.minix_i.u.i1_data[i]) {
			minix_free_block(inode->i_sb, inode->u.minix_i.u.i1_data[i]);
			inode->u.minix_i.u.i1_data[i] = 0;
		}
	}
}

/*
 * Free direct blocks.
 */
static void V2_minix_free_direct_block(struct inode *inode)
{
	int i;

	for (i = DIRECT_BLOCK(inode); i < 7; i++) {
		if (inode->u.minix_i.u.i2_data[i]) {
			minix_free_block(inode->i_sb, inode->u.minix_i.u.i2_data[i]);
			inode->u.minix_i.u.i2_data[i] = 0;
		}
	}
}

/*
 * Free single indirect blocks.
 */
static void V1_minix_free_indirect_blocks(struct inode *inode, int offset, uint16_t *block)
{
	struct buffer_head *bh;
	uint16_t *blocks;
	int i;

	if (!*block)
		return;

	/* get block */
	bh = bread(inode->i_dev, *block, inode->i_sb->s_blocksize);
	if (!bh)
		return;

	/* free all pointed blocks */
	blocks = (uint16_t *) bh->b_data;
	for (i = INDIRECT_BLOCK(inode, offset); i < 512; i++) {
		if (i < 0)
			i = 0;

		if (blocks[i])
			minix_free_block(inode->i_sb, blocks[i]);
	}

	/* get first used adress */
	for (i = 0; i < 512; i++)
		if (blocks[i])
			break;

	/* indirect block not used anymore : free it */
	if (i >= 512) {
		minix_free_block(inode->i_sb, *block);
		*block = 0;
	}

	/* release buffer */
	brelse(bh);
}

/*
 * Free single indirect blocks.
 */
static void V2_minix_free_indirect_blocks(struct inode *inode, int offset, uint32_t *block)
{
	struct buffer_head *bh;
	uint32_t *blocks;
	int i;

	if (!*block)
		return;

	/* get block */
	bh = bread(inode->i_dev, *block, inode->i_sb->s_blocksize);
	if (!bh)
		return;

	/* free all pointed blocks */
	blocks = (uint32_t *) bh->b_data;
	for (i = INDIRECT_BLOCK(inode, offset); i < 256; i++) {
		if (i < 0)
			i = 0;

		if (blocks[i])
			minix_free_block(inode->i_sb, blocks[i]);
	}

	/* get first used adress */
	for (i = 0; i < 256; i++)
		if (blocks[i])
			break;

	/* indirect block not used anymore : free it */
	if (i >= 256) {
		minix_free_block(inode->i_sb, *block);
		*block = 0;
	}

	/* release buffer */
	brelse(bh);
}

/*
 * Free double indirect blocks.
 */
static void V1_minix_free_dindirect_blocks(struct inode *inode, int offset, uint16_t *block)
{
	struct buffer_head *bh;
	uint16_t *blocks;
	int i;

	if (!*block)
		return;

	/* get block */
	bh = bread(inode->i_dev, *block, inode->i_sb->s_blocksize);
	if (!bh)
		return;

	/* free all pointed blocks */
	blocks = (uint16_t *) bh->b_data;
	for (i = V1_DINDIRECT_BLOCK(inode, offset); i < 512; i++) {
		if (i < 0)
			i = 0;

		if (blocks[i])
			V1_minix_free_indirect_blocks(inode, offset + (i << 9), &blocks[i]);
	}

	/* get first used adress */
	for (i = 0; i < 512; i++)
		if (blocks[i])
			break;

	/* indirect block not used anymore : free it */
	if (i >= 512) {
		minix_free_block(inode->i_sb, *block);
		*block = 0;
	}

	/* release buffer */
	brelse(bh);
}

/*
 * Free double indirect blocks.
 */
static void V2_minix_free_dindirect_blocks(struct inode *inode, int offset, uint32_t *block)
{
	struct buffer_head *bh;
	uint32_t *blocks;
	int i;

	if (!*block)
		return;

	/* get block */
	bh = bread(inode->i_dev, *block, inode->i_sb->s_blocksize);
	if (!bh)
		return;

	/* free all pointed blocks */
	blocks = (uint32_t *) bh->b_data;
	for (i = V2_DINDIRECT_BLOCK(inode, offset); i < 256; i++) {
		if (i < 0)
			i = 0;

		if (blocks[i])
			V2_minix_free_indirect_blocks(inode, offset + (i << 8), &blocks[i]);
	}

	/* get first used adress */
	for (i = 0; i < 256; i++)
		if (blocks[i])
			break;

	/* indirect block not used anymore : free it */
	if (i >= 256) {
		minix_free_block(inode->i_sb, *block);
		*block = 0;
	}

	/* release buffer */
	brelse(bh);
}

/*
 * Free triple indirect blocks.
 */
static void V2_minix_free_tindirect_blocks(struct inode *inode, int offset, uint32_t *block)
{
	struct buffer_head *bh;
	uint32_t *blocks;
	int i;

	if (!block)
		return;

	/* get block */
	bh = bread(inode->i_dev, *block, inode->i_sb->s_blocksize);
	if (!bh)
		return;

	/* free all pointed blocks */
	blocks = (uint32_t *) bh->b_data;
	for (i = TINDIRECT_BLOCK(inode, offset); i < 256; i++) {
		if (i < 0)
			i = 0;

		if (blocks[i])
			V2_minix_free_dindirect_blocks(inode, offset + (i << 8), &blocks[i]);
	}

	/* get first used adress */
	for (i = 0; i < 256; i++)
		if (blocks[i])
			break;

	/* indirect block not used anymore : free it */
	if (i >= 256) {
		minix_free_block(inode->i_sb, *block);
		*block = 0;
	}

	/* release buffer */
	brelse(bh);
}

/*
 * Truncate a V1 inode.
 */
static void V1_minix_truncate(struct inode *inode)
{
	/* only allowed on regular files and directories */
	if (!inode || !(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode)))
		return;

	/* free direct blocks */
	V1_minix_free_direct_block(inode);

	/* free indirect blocks */
	V1_minix_free_indirect_blocks(inode, 7, &inode->u.minix_i.u.i1_data[7]);

	/* free double indirect blocks */
	V1_minix_free_dindirect_blocks(inode, 7 + 512, &inode->u.minix_i.u.i1_data[8]);

	/* mark inode dirty */
	mark_inode_dirty(inode);
}

/*
 * Truncate a V2 inode.
 */
static void V2_minix_truncate(struct inode *inode)
{
	/* only allowed on regular files and directories */
	if (!inode || !(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode)))
		return;

	/* free direct blocks */
	V2_minix_free_direct_block(inode);

	/* free indirect blocks */
	V2_minix_free_indirect_blocks(inode, 7, &inode->u.minix_i.u.i2_data[7]);

	/* free double indirect blocks */
	V2_minix_free_dindirect_blocks(inode, 7 + 256, &inode->u.minix_i.u.i2_data[8]);

	/* free triple indirect blocks */
	V2_minix_free_tindirect_blocks(inode, 7 + 256 + 256 * 256, &inode->u.minix_i.u.i2_data[9]);

	/* mark inode dirty */
	mark_inode_dirty(inode);
}

/*
 * Truncate an inode.
 */
void minix_truncate(struct inode *inode)
{
	if (minix_sb(inode->i_sb)->s_version == MINIX_V1)
		V1_minix_truncate(inode);
	else
		V2_minix_truncate(inode);
}
