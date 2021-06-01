#include <fs/fs.h>
#include <fcntl.h>

/*
 * Free single indirect blocks.
 */
static void minix_free_indirect_blocks(struct super_block_t *sb, int block)
{
  struct buffer_head_t *bh;
  uint16_t *blocks;
  int i;

  if (!block)
    return;

  /* get block */
  bh = bread(sb->s_dev, block);
  if (!bh)
    return;

  /* free all pointed blocks */
  blocks = (uint16_t *) bh->b_data;
  for (i = 0; i < BLOCK_SIZE / 2; i++)
    if (blocks[i])
      free_block(sb, blocks[i]);

  /* release buffer */
  brelse(bh);

  /* free this block */
  free_block(sb, block);
}

/*
 * Free double indirect blocks.
 */
static void minix_free_double_indirect_blocks(struct super_block_t *sb, int block)
{
  struct buffer_head_t *bh;
  uint16_t *blocks;
  int i;

  if (!block)
    return;

  /* get block */
  bh = bread(sb->s_dev, block);
  if (!bh)
    return;

  /* free all pointed blocks */
  blocks = (uint16_t *) bh->b_data;
  for (i = 0; i < BLOCK_SIZE / 2; i++)
    if (blocks[i])
      minix_free_indirect_blocks(sb, blocks[i]);

  /* release buffer */
  brelse(bh);

  /* free this block */
  free_block(sb, block);
}

/*
 * Truncate an inode.
 */
void minix_truncate(struct inode_t *inode)
{
  int i;

  /* only allowed on regular files and directories */
  if (!inode || !(S_ISREG(inode->i_mode) || S_ISDIR(inode->i_mode)))
    return;

  /* free direct blocks */
  for (i = 0; i < 7; i++) {
    if (inode->i_zone[i]) {
      free_block(inode->i_sb, inode->i_zone[i]);
      inode->i_zone[i] = 0;
    }
  }

  /* free indirect blocks */
  minix_free_indirect_blocks(inode->i_sb, inode->i_zone[7]);
  inode->i_zone[7] = 0;

  /* free double indirect blocks */
  minix_free_double_indirect_blocks(inode->i_sb, inode->i_zone[8]);
  inode->i_zone[8] = 0;

  /* update inode size */
  inode->i_size = 0;
  inode->i_dirt = 1;
}
