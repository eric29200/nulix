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

  /* allocate data */
  bh->b_data = (char *) kmalloc(BLOCK_SIZE);
  if (!bh->b_data) {
    kfree(bh);
    return NULL;
  }

  /* set buffer */
  bh->b_dev = dev;
  bh->b_blocknr = block;
  memset(bh->b_data, 0, sizeof(struct buffer_head_t));

  /* read from device */
  if (ata_read(dev, sector, nb_sectors, (uint16_t *) bh->b_data) != 0) {
    brelse(bh);
    return NULL;
  }

  return bh;
}

/*
 * Release a buffer.
 */
void brelse(struct buffer_head_t *bh)
{
  if (bh) {
    if (bh->b_data)
      kfree(bh->b_data);
    kfree(bh);
  }
}
