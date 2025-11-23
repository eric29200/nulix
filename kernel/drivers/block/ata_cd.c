#include <drivers/block/ata.h>
#include <x86/io.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Wait for operation completion.
 */
static int ata_cd_wait(struct ata_device *device)
{
	uint8_t status;

	for (;;) {
		status = inb(device->io_base + ATA_REG_STATUS);
		if (!status)
			return -ENXIO;

		if (status & ATA_SR_ERR)
			return -EIO;

		if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ))
			break;
	}

	return 0;
}

/*
 * Read a sector from an ata device.
 */
static int ata_cd_read_sector(struct ata_device *device, uint32_t sector, char *buf)
{
	uint8_t command[12];
	int ret;

	/* select drive */
	outb(device->io_base + ATA_REG_HDDEVSEL, device->drive == ATA_MASTER ? 0xE0 : 0xF0);
	outb(device->io_base + ATA_REG_FEATURES, 0x00);

	/* issue packet command */
	outb(device->io_base + ATA_REG_LBA1, (uint8_t) (ATAPI_SECTOR_SIZE & 0xFF));
	outb(device->io_base + ATA_REG_LBA2, (uint8_t) (ATAPI_SECTOR_SIZE >> 8));
	outb(device->io_base + ATA_REG_COMMAND, ATA_CMD_PACKET);

	/* wait for completion */
	ret = ata_cd_wait(device);
	if (ret)
		return ret;

	/* prepare read command */
	memset(command, 0, 12);
	command[0] = 0xA8;
	command[2] = (sector >> 24) & 0xFF;
	command[3] = (sector >> 16) & 0xFF;
	command[4] = (sector >> 8) & 0xFF;
	command[5] = sector & 0xFF;
	command[9] = 1;

	/* issue read command */
	outsw(device->io_base, command, 12 / sizeof(uint16_t));

	/* wait for completion */
	ret = ata_cd_wait(device);
	if (ret)
		return ret;

	/* read data */
	insw(device->io_base, buf, ATAPI_SECTOR_SIZE / sizeof(uint16_t));

	return 0;
}

/*
 * Read from an ata device.
 */
static int ata_cd_read(struct ata_device *device, uint32_t sector, size_t nr_sectors, char *buf)
{
	size_t i;
	int ret;

	/* read sectors */
	for (i = 0; i < nr_sectors; i++) {
		ret = ata_cd_read_sector(device, sector + i, buf + ATAPI_SECTOR_SIZE * i);
		if (ret)
			return ret;
	}

	return 0;
}

/*
 * Init an ata cd device.
 */
int ata_cd_init(struct ata_device *device)
{
	device->sector_size = ATAPI_SECTOR_SIZE;
	device->read = ata_cd_read;
	device->write = NULL;

	return 0;
}
