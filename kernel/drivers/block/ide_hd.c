#include <drivers/block/ide.h>
#include <x86/io.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Wait for operation completion.
 */
static int ide_hd_wait(struct ide_drive *drive)
{
	uint8_t status;

	for (;;) {
		status = inb(drive->io_base + ATA_REG_STATUS);
		if (!status)
			return -ENXIO;

		if (status & ATA_SR_ERR)
			return -EIO;

		if (!(status & ATA_SR_BSY))
			break;
	}

	return 0;
}

/*
 * Read/write a sector in pio mode.
 */
static int ide_hd_rw_sector(struct ide_drive *drive, int cmd, uint32_t sector, char *buf)
{
	int ret;

	/* select sector */
	outb(drive->io_base + ATA_REG_CONTROL, 0x02);
	outb(drive->io_base + ATA_REG_HDDEVSEL, (drive->master ? 0xE0 : 0xF0) | ((sector >> 24) & 0x0F));
	outb(drive->io_base + ATA_REG_FEATURES, 0x00);
	outb(drive->io_base + ATA_REG_SECCOUNT0, 1);
	outb(drive->io_base + ATA_REG_LBA0, (uint8_t) sector);
	outb(drive->io_base + ATA_REG_LBA1, (uint8_t) (sector >> 8));
	outb(drive->io_base + ATA_REG_LBA2, (uint8_t) (sector >> 16));

	/* issue read/write command */
	outb(drive->io_base + ATA_REG_COMMAND, cmd == READ ? ATA_CMD_READ_PIO : ATA_CMD_WRITE_PIO);

	/* wait for disk to be ready */
	ret = ide_hd_wait(drive);
	if (ret)
		return ret;

	/* read or write data */
	if (cmd == READ) {
		insw(drive->io_base + ATA_REG_DATA, buf, ATA_SECTOR_SIZE / 2);
	} else {
		outsw(drive->io_base + ATA_REG_DATA, buf, ATA_SECTOR_SIZE / 2);
		outb(drive->io_base + ATA_REG_COMMAND, ATA_CMD_CACHE_FLUSH);
	}

	/* wait for drive */
	ret = ide_hd_wait(drive);
	if (ret)
		return ret;

	return 0;
}

/*
 * Do read/write in PIO mode.
 */
static int ide_do_rw_disk_pio(struct ide_drive *drive, struct request *req)
{
	uint32_t sector, start_sector;
	struct buffer_head *bh;
	struct list_head *pos;
	size_t i;
	int ret;

	/* get partition start sector */
	start_sector = drive->part[minor(req->rq_dev) & PARTITION_MINOR_MASK].start_sect;
	sector = start_sector + (req->sector << 9) / ATA_SECTOR_SIZE;

	/* read/write buffers */
	list_for_each(pos, &req->bhs_list) {
		bh = list_entry(pos, struct buffer_head, b_list_req);

		/* read/write sectors */
		for (i = 0; i < bh->b_size / ATA_SECTOR_SIZE; i++) {
			ret = ide_hd_rw_sector(drive, req->cmd, sector++, bh->b_data + i * ATA_SECTOR_SIZE);
			if (ret)
				return ret;
		}
	}

	return 0;
}

/*
 * Do read/write.
 */
int ide_do_rw_disk(struct ide_drive *drive, struct request *req)
{
	uint32_t start_sector, sector, nr_sectors;
	int status, dstatus;

	/* get partition start sector */
	start_sector = drive->part[minor(req->rq_dev) & PARTITION_MINOR_MASK].start_sect;
	sector = start_sector + (req->sector << 9) / ATA_SECTOR_SIZE;
	nr_sectors = (req->nr_sectors << 9) / ATA_SECTOR_SIZE;

	/* check command */
	if (req->cmd != READ && req->cmd != WRITE) {
		printf("ide_do_rw_disk: can't handle request %x\n", req->cmd);
		return -EIO;
	}

	/* select sector */
	outb(drive->io_base + ATA_REG_CONTROL, 0x00);
	outb(drive->io_base + ATA_REG_HDDEVSEL, (drive->master ? 0xE0 : 0xF0) | ((sector >> 24) & 0x0F));
	outb(drive->io_base + ATA_REG_FEATURES, 0x00);
	outb(drive->io_base + ATA_REG_SECCOUNT0, nr_sectors);
	outb(drive->io_base + ATA_REG_LBA0, (uint8_t) sector);
	outb(drive->io_base + ATA_REG_LBA1, (uint8_t) (sector >> 8));
	outb(drive->io_base + ATA_REG_LBA2, (uint8_t) (sector >> 16));

	/* issue dma command : on failure try pio mode */
	if (ide_dmaproc(drive, req))
		return ide_do_rw_disk_pio(drive, req);

	/* wait for completion */
	for (;;) {
		status = inb(drive->hwif->dma_base + 2);
		dstatus = inb(drive->io_base + ATA_REG_STATUS);

		if (!(status & 0x04))
			continue;

		if (!(dstatus & ATA_SR_BSY))
			break;
	}

	return 0;
}
