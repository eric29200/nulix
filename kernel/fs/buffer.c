#include <fs/fs.h>
#include <mm/mm.h>
#include <string.h>
#include <stderr.h>

/* global buffer table */
static struct buffer_head_t buffer_table[NR_BUFFER];

/*
 * Get an empty buffer.
 */
static struct buffer_head_t *get_empty_buffer()
{
  struct buffer_head_t *bh;
  int i;

  /* find a free buffer */
  for (i = 0; i < NR_BUFFER; i++) {
    if (!buffer_table[i].b_ref) {
      bh = &buffer_table[i];
      break;
    }
  }

  /* no more buffer */
  if (!bh)
    return NULL;

  /* reset inode */
  memset(bh, 0, sizeof(struct buffer_head_t));
  bh->b_ref = 1;

  return bh;
}

/*
 * Read a block from a device.
 */
struct buffer_head_t *bread(struct ata_device_t *dev, uint32_t block)
{
  uint32_t nb_sectors, sector;
  struct buffer_head_t *bh;
  int i;

  /* compute nb sectors */
  nb_sectors = BLOCK_SIZE / ATA_SECTOR_SIZE;
  sector = block * BLOCK_SIZE / ATA_SECTOR_SIZE;

  /* try to find buffer in table */
  for (i = 0; i < NR_BUFFER; i++) {
    if (buffer_table[i].b_blocknr == block) {
      bh = &buffer_table[i];
      bh->b_ref++;
      return bh;
    }
  }

  /* get an empty buffer */
  bh = get_empty_buffer();
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

  /* update inode reference count */
  bh->b_ref--;
  if (bh->b_ref == 0)
    memset(bh, 0, sizeof(struct buffer_head_t));
}
