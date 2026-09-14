#include <drivers/block/ide.h>
#include <x86/io.h>
#include <stderr.h>
#include <stdio.h>

#define NR_DMA_SECTORS			((IDE_NR_DMA_PAGES) * PAGE_SIZE / ATA_SECTOR_SIZE)

/*
 * Issue read/write command.
 */
static void __ide_do_rw_disk(struct ide_drive *drive, int cmd, uint32_t sector, size_t nr_sectors)
{
	int status, dstatus;

	/* select sector */
	outb(drive->io_base + ATA_REG_CONTROL, 0x00);
	outb(drive->io_base + ATA_REG_HDDEVSEL, (drive->drive == ATA_MASTER ? 0xE0 : 0xF0) | ((sector >> 24) & 0x0F));
	outb(drive->io_base + ATA_REG_FEATURES, 0x00);
	outb(drive->io_base + ATA_REG_SECCOUNT0, nr_sectors);
	outb(drive->io_base + ATA_REG_LBA0, (uint8_t) sector);
	outb(drive->io_base + ATA_REG_LBA1, (uint8_t) (sector >> 8));
	outb(drive->io_base + ATA_REG_LBA2, (uint8_t) (sector >> 16));

	/* issue dma command */
	ide_dmaproc(drive, cmd, nr_sectors * ATA_SECTOR_SIZE);

	/* wait for completion */
	for (;;) {
		status = inb(drive->hwif->dma_base + 2);
		dstatus = inb(drive->io_base + ATA_REG_STATUS);

		if (!(status & 0x04))
			continue;

		if (!(dstatus & ATA_SR_BSY))
			break;
	}
}

/*
 * Do read/write.
 */
int ide_do_rw_disk(struct ide_drive *drive, struct request *req)
{
	uint32_t start_sector, sector, nr_sectors, nsect;
	char *buf = req->buf;

	/* get partition start sector */
	start_sector = drive->part[minor(req->rq_dev) & PARTITION_MINOR_MASK].start_sect;
	sector = start_sector + (req->sector << 9) / ATA_SECTOR_SIZE;
	nr_sectors = (req->nr_sectors << 9) / ATA_SECTOR_SIZE;

	/* check command */
	if (req->cmd != READ && req->cmd != WRITE) {
		printf("ide_do_rw_disk: can't handle request %x\n", req->cmd);
		return -EIO;
	}

	/* read/write */
	while (nr_sectors) {
		/* limit transfert to buffer size */
		nsect = nr_sectors;
		if (nsect > NR_DMA_SECTORS)
			nsect = NR_DMA_SECTORS;

		/* copy buffer */
		if (req->cmd == WRITE)
			memcpy(drive->buf, buf, nsect * ATA_SECTOR_SIZE);

		/* issue read/write command */
		__ide_do_rw_disk(drive, req->cmd == READ ? ATA_CMD_READ_DMA : ATA_CMD_WRITE_DMA, sector, nsect);

		/* copy buffer */
		if (req->cmd == READ)
			memcpy(buf, drive->buf, nsect * ATA_SECTOR_SIZE);

		/* update size */
		buf += nsect * ATA_SECTOR_SIZE;
		sector += nsect;
		nr_sectors -= nsect;
	}

	return 0;
}