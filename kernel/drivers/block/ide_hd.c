#include <drivers/block/ide.h>
#include <x86/io.h>
#include <stderr.h>
#include <stdio.h>

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
	outb(drive->io_base + ATA_REG_HDDEVSEL, (drive->drive == ATA_MASTER ? 0xE0 : 0xF0) | ((sector >> 24) & 0x0F));
	outb(drive->io_base + ATA_REG_FEATURES, 0x00);
	outb(drive->io_base + ATA_REG_SECCOUNT0, nr_sectors);
	outb(drive->io_base + ATA_REG_LBA0, (uint8_t) sector);
	outb(drive->io_base + ATA_REG_LBA1, (uint8_t) (sector >> 8));
	outb(drive->io_base + ATA_REG_LBA2, (uint8_t) (sector >> 16));

	/* issue dma command */
	ide_dmaproc(drive, req);

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