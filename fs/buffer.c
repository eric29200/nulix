#include <fs/buffer.h>
#include <mm/mm.h>
#include <string.h>

/*
 * Read a block from a device.
 */
char *bread(struct ata_device_t *dev, uint32_t block)
{
  uint32_t nb_sectors, sector;
  char *buffer;

  /* compute nb sectors */
  nb_sectors = div_ceil(BLOCK_SIZE, ATA_SECTOR_SIZE);
  sector = block * div_floor(BLOCK_SIZE, ATA_SECTOR_SIZE);

  /* allocate buffer */
  buffer = (char *) kmalloc(nb_sectors * ATA_SECTOR_SIZE);
  if (!buffer)
    return NULL;
  memset(buffer, 0, nb_sectors * ATA_SECTOR_SIZE);

  /* read from device */
  if (ata_read(dev, sector, nb_sectors, (uint16_t *) buffer) != 0) {
    kfree(buffer);
    return NULL;
  }

  return buffer;
}

/*
 * Write a block to a device.
 */
int bwrite(struct ata_device_t *dev, uint32_t block, const char *buffer)
{
  uint32_t nb_sectors, sector;

  /* compute nb sectors */
  nb_sectors = div_ceil(BLOCK_SIZE, ATA_SECTOR_SIZE);
  sector = block * div_floor(BLOCK_SIZE, ATA_SECTOR_SIZE);

  /* write to device */
  return ata_write(dev, sector, nb_sectors, (uint16_t *) buffer);
}
