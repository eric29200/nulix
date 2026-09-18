#include <drivers/block/ide.h>
#include <x86/io.h>
#include <stderr.h>
#include <stdio.h>
#include <dev.h>

/*
 * Wait for operation completion.
 */
static int ide_cd_wait(struct ide_drive *drive)
{
	uint8_t status;

	for (;;) {
		status = inb(drive->io_base + ATA_REG_STATUS);
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
 * Read a sector from an IDE cd drive.
 */
static int ide_cd_read_sector(struct ide_drive *drive, uint32_t sector, char *buf)
{
	uint8_t command[12];
	int ret;

	/* select drive */
	outb(drive->io_base + ATA_REG_HDDEVSEL, drive->master ? 0xE0 : 0xF0);
	outb(drive->io_base + ATA_REG_FEATURES, 0x00);

	/* issue packet command */
	outb(drive->io_base + ATA_REG_LBA1, (uint8_t) (ATAPI_SECTOR_SIZE & 0xFF));
	outb(drive->io_base + ATA_REG_LBA2, (uint8_t) (ATAPI_SECTOR_SIZE >> 8));
	outb(drive->io_base + ATA_REG_COMMAND, ATA_CMD_PACKET);

	/* wait for completion */
	ret = ide_cd_wait(drive);
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
	outsw(drive->io_base, command, 12 / sizeof(uint16_t));

	/* wait for completion */
	ret = ide_cd_wait(drive);
	if (ret)
		return ret;

	/* read data */
	insw(drive->io_base, buf, ATAPI_SECTOR_SIZE / sizeof(uint16_t));

	return 0;
}

/*
 * Do read/write.
 */
int ide_do_rw_cdrom(struct ide_drive *drive, struct request *req)
{
	uint32_t start_sector, sector;
	struct buffer_head *bh;
	struct list_head *pos;
	int ret;

	/* get partition start sector */
	start_sector = drive->part[minor(req->rq_dev) & PARTITION_MINOR_MASK].start_sect;
	sector = start_sector + (req->sector << 9) / ATAPI_SECTOR_SIZE;

	/* read only  */
	if (req->cmd != READ) {
		printf("ide_do_rw_cdrom: can't handle request %x\n", req->cmd);
		return -EIO;
	}

	/* read buffers */
	list_for_each(pos, &req->bhs_list) {
		bh = list_entry(pos, struct buffer_head, b_list_req);

		/* read sector */
		ret = ide_cd_read_sector(drive, sector++, bh->b_data);
		if (ret)
			return ret;
	}

	return 0;
}