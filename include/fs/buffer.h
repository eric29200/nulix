#ifndef _FS_BUFFER_H_
#define _FS_BUFFER_H_

#include <drivers/ata.h>

#define BLOCK_SIZE      1024

char *bread(struct ata_device_t *dev, uint32_t block);
int bwrite(struct ata_device_t *dev, uint32_t block, const char *buffer);

#endif
