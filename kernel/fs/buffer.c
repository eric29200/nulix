#include <fs/fs.h>
#include <mm/mm.h>
#include <string.h>
#include <stderr.h>

/*
 * Read a block from a device.
 */
struct buffer_head_t *bread(struct ata_device_t *dev, uint32_t block)
{
  uint32_t nb_sectors, sector;
  struct buffer_head_t *bh;

  /* compute nb sectors */
  nb_sectors = BLOCK_SIZE / ATA_SECTOR_SIZE;
  sector = block * BLOCK_SIZE / ATA_SECTOR_SIZE;

  /* allocate buffer */
  bh = (struct buffer_head_t *) kmalloc(sizeof(struct buffer_head_t));
  if (!bh)
    return NULL;

  /* set buffer */
  bh->b_dev = dev;
  bh->b_blocknr = block;
  bh->b_dirt = 0;
  memset(bh->b_data, 0, BLOCK_SIZE);

  /* read from device */
  if (ata_read(dev, sector, nb_sectors, (uint16_t *) bh->b_data) != 0) {
    brelse(bh);
    return NULL;
  }

  return bh;
}

/*
 * Write a block buffer.
 */
int bwrite(struct buffer_head_t *bh)
{
  uint32_t nb_sectors, sector;

  if (!bh)
    return -EINVAL;

  /* compute nb sectors */
  nb_sectors = BLOCK_SIZE / ATA_SECTOR_SIZE;
  sector = bh->b_blocknr * BLOCK_SIZE / ATA_SECTOR_SIZE;

  /* write to block device */
  return ata_write(bh->b_dev, sector, nb_sectors, (uint16_t *) bh->b_data);
}

/*
 * Release a buffer.
 */
void brelse(struct buffer_head_t *bh)
{
  if (!bh)
    return;

  /* write dirty buffer */
  if (bh->b_dirt) {
    bwrite(bh);
    bh->b_dirt = 0;
  }

  /* free buffer */
  kfree(bh);
}
