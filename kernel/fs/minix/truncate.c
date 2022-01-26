#include <fs/minix_fs.h>
#include <fcntl.h>

#define DIRECT_BLOCK(inode)                   (((inode)->i_size + 1023) >> 10)
#define INDIRECT_BLOCK(inode, offset)         (DIRECT_BLOCK((inode)) - offset)
#define DINDIRECT_BLOCK(inode, offset)        ((DIRECT_BLOCK((inode)) - offset) >> 8)
#define TINDIRECT_BLOCK(inode, offset)        ((DIRECT_BLOCK((inode)) - offset) >> 8)

/*
 * Free direct blocks.
 */
static void minix_free_direct_block(struct inode_t *inode)
{
  int i;

  for (i = DIRECT_BLOCK(inode); i < 7; i++) {
    if (inode->i_zone[i]) {
      minix_free_block(inode->i_sb, inode->i_zone[i]);
      inode->i_zone[i] = 0;
    }
  }
}

/*
 * Free single indirect blocks.
 */
static void minix_free_indirect_blocks(struct inode_t *inode, int offset, uint32_t *block)
{
  struct buffer_head_t *bh;
  uint32_t *blocks;
  int i;

  if (!*block)
    return;

  /* get block */
  bh = bread(inode->i_sb->s_dev, *block);
  if (!bh)
    return;

  /* free all pointed blocks */
  blocks = (uint32_t *) bh->b_data;
  for (i = INDIRECT_BLOCK(inode, offset); i < BLOCK_SIZE / 4; i++)
    if (blocks[i])
      minix_free_block(inode->i_sb, blocks[i]);

  /* get first used adress */
  for (i = 0; i < BLOCK_SIZE / 4; i++)
    if (blocks[i])
      break;

  /* indirect block not used anymore : free it */
  if (i >= BLOCK_SIZE / 4) {
    minix_free_block(inode->i_sb, *block);
    *block = 0;
  }

  /* release buffer */
  brelse(bh);
}

/*
 * Free double indirect blocks.
 */
static void minix_free_dindirect_blocks(struct inode_t *inode, int offset, uint32_t *block)
{
  struct buffer_head_t *bh;
  uint32_t *blocks;
  int i;

  if (!*block)
    return;

  /* get block */
  bh = bread(inode->i_sb->s_dev, *block);
  if (!bh)
    return;

  /* free all pointed blocks */
  blocks = (uint32_t *) bh->b_data;
  for (i = DINDIRECT_BLOCK(inode, offset); i < BLOCK_SIZE / 4; i++)
    if (blocks[i])
      minix_free_indirect_blocks(inode, offset + (i << 8), &blocks[i]);

  /* get first used adress */
  for (i = 0; i < BLOCK_SIZE / 4; i++)
    if (blocks[i])
      break;

  /* indirect block not used anymore : free it */
  if (i >= BLOCK_SIZE / 4) {
    minix_free_block(inode->i_sb, *block);
    *block = 0;
  }

  /* release buffer */
  brelse(bh);
}

/*
 * Free triple indirect blocks.
 */
static void minix_free_tindirect_blocks(struct inode_t *inode, int offset, uint32_t *block)
{
  struct buffer_head_t *bh;
  uint32_t *blocks;
  int i;

  if (!block)
    return;

  /* get block */
  bh = bread(inode->i_sb->s_dev, *block);
  if (!bh)
    return;

  /* free all pointed blocks */
  blocks = (uint32_t *) bh->b_data;
  for (i = TINDIRECT_BLOCK(inode, offset); i < BLOCK_SIZE / 4; i++)
    if (blocks[i])
      minix_free_dindirect_blocks(inode, offset + (i << 8), &blocks[i]);

  /* get first used adress */
  for (i = 0; i < BLOCK_SIZE / 4; i++)
    if (blocks[i])
      break;

  /* indirect block not used anymore : free it */
  if (i >= BLOCK_SIZE / 4) {
    minix_free_block(inode->i_sb, *block);
    *block = 0;
  }

  /* release buffer */
  brelse(bh);
}

/*
 * Truncate an inode.
 */
void minix_truncate(struct inode_t *inode)
{
  /* only allowed on regular files and directories */
  if (!inode || !(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode)))
    return;

  /* free direct blocks */
  minix_free_direct_block(inode);

  /* free indirect blocks */
  minix_free_indirect_blocks(inode, 7, &inode->i_zone[7]);

  /* free double indirect blocks */
  minix_free_dindirect_blocks(inode, 7 + 256, &inode->i_zone[8]);

  /* free triple indirect blocks */
  minix_free_tindirect_blocks(inode, 7 + 256 + 256 * 256, &inode->i_zone[9]);

  /* mark inode dirty */
  inode->i_dirt = 1;
}
