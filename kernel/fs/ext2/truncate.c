#include <fs/fs.h>
#include <fs/ext2_fs.h>
#include <fcntl.h>

#define DIRECT_BLOCK(inode)			(((inode)->i_size + (inode)->i_sb->s_blocksize - 1) / (inode)->i_sb->s_blocksize)
#define INDIRECT_BLOCK(inode, offset)		((int) DIRECT_BLOCK((inode)) - offset)
#define DINDIRECT_BLOCK(inode, offset)		((int) DIRECT_BLOCK(inode) / addr_per_block)
#define TINDIRECT_BLOCK(inode, offset)		(((int) DIRECT_BLOCK(inode) - (addr_per_block * addr_per_block 	\
						+ addr_per_block + EXT2_NDIR_BLOCKS)) 				\
						/ (addr_per_block * addr_per_block))

/*
 * Free Ext2 direct blocks.
 */
static void ext2_free_direct_blocks(struct inode *inode)
{
	struct ext2_inode_info *ext2_inode = &inode->u.ext2_i;
	int i;

	/* free direct blocks */
	for (i = DIRECT_BLOCK(inode); i < EXT2_NDIR_BLOCKS; i++) {
		if (i < 0)
			i = 0;

		/* free block */
		if (ext2_inode->i_data[i]) {
			ext2_free_block(inode, ext2_inode->i_data[i]);
			ext2_inode->i_data[i] = 0;
		}
	}
}

/*
 * Free Ext2 single indirect blocks.
 */
static void ext2_free_indirect_blocks(struct inode *inode, int offset, uint32_t *block, int addr_per_block)
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
	blocks = (uint32_t * ) bh->b_data;
	for (i = INDIRECT_BLOCK(inode, offset); i < addr_per_block; i++) {
		if (i < 0)
			i = 0;

		/* free block */
		if (blocks[i])
			ext2_free_block(inode, blocks[i]);

		/* mark parent block dirty */
		blocks[i] = 0;
		mark_buffer_dirty(bh);
	}

	/* get first used address */
	for (i = 0; i < addr_per_block; i++)
		if (blocks[i])
			break;

	/* indirect block not used anymore : free it */
	if (i >= addr_per_block) {
		ext2_free_block(inode, *block);
		*block = 0;
	}

	/* release buffer */
	brelse(bh);
}

/*
 * Free Ext2 double indirect blocks.
 */
static void ext2_free_dindirect_blocks(struct inode *inode, int offset, uint32_t *block, int addr_per_block)
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
	blocks = (uint32_t * ) bh->b_data;
	for (i = DINDIRECT_BLOCK(inode, offset); i < addr_per_block; i++) {
		if (i < 0)
			i = 0;

		/* free block */
		if (blocks[i])
			ext2_free_indirect_blocks(inode, offset + (i / addr_per_block), &blocks[i], addr_per_block);

		/* mark parent block dirty */
		blocks[i] = 0;
		mark_buffer_dirty(bh);
	}

	/* get first used address */
	for (i = 0; i < addr_per_block; i++)
		if (blocks[i])
			break;

	/* indirect block not used anymore : free it */
	if (i >= addr_per_block) {
		ext2_free_block(inode, *block);
		*block = 0;
	}

	/* release buffer */
	brelse(bh);
}

/*
 * Free Ext2 triple indirect blocks.
 */
static void ext2_free_tindirect_blocks(struct inode *inode, int offset, uint32_t *block, int addr_per_block)
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
	blocks = (uint32_t * ) bh->b_data;
	for (i = TINDIRECT_BLOCK(inode, offset); i < addr_per_block; i++) {
		if (i < 0)
			i = 0;

		/* free block */
		if (blocks[i])
			ext2_free_dindirect_blocks(inode, offset + (i / addr_per_block), &blocks[i], addr_per_block);

		/* mark parent block dirty */
		blocks[i] = 0;
		mark_buffer_dirty(bh);
	}

	/* get first used address */
	for (i = 0; i < addr_per_block; i++)
		if (blocks[i])
			break;

	/* indirect block not used anymore : free it */
	if (i >= addr_per_block) {
		ext2_free_block(inode, *block);
		*block = 0;
	}

	/* release buffer */
	brelse(bh);
}

/*
 * Truncate a Ext2 inode.
 */
void ext2_truncate(struct inode *inode)
{
	struct ext2_inode_info *ext2_inode = &inode->u.ext2_i;
	int addr_per_block;

	/* only allowed on regular files and directories */
	if (!inode || !(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode)))
		return;

	/* compute number of addressed per block */
	addr_per_block = inode->i_sb->s_blocksize / 4;

	/* free direct, indirect, double indirect and triple indirect blocks */
	ext2_free_direct_blocks(inode);
	ext2_free_indirect_blocks(inode, EXT2_NDIR_BLOCKS, &ext2_inode->i_data[EXT2_IND_BLOCK], addr_per_block);
	ext2_free_dindirect_blocks(inode, EXT2_NDIR_BLOCKS + addr_per_block, &ext2_inode->i_data[EXT2_DIND_BLOCK], addr_per_block);
	ext2_free_tindirect_blocks(inode, EXT2_NDIR_BLOCKS + addr_per_block + addr_per_block * addr_per_block,
				   &ext2_inode->i_data[EXT2_TIND_BLOCK], addr_per_block);

	/* mark inode dirty */
	inode->i_mtime = inode->i_ctime = CURRENT_TIME;
	mark_inode_dirty(inode);
}
