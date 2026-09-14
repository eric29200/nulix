#include <drivers/block/ide.h>
#include <x86/io.h>
#include <stderr.h>
#include <stdio.h>

#define NR_DMA_SECTORS			((NR_DMA_PAGES) * PAGE_SIZE / ATA_SECTOR_SIZE)

/*
 * Wait for operation completion.
 */
static void ide_hd_wait(struct ide_drive *drive)
{
	int status, dstatus;

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
 * Issue a DMA command.
 */
static void ide_hd_dmaproc(struct ide_drive *drive, int cmd, uint32_t sector, size_t nr_sectors)
{
	uint32_t dma_base = drive->hwif->dma_base;

	/* set transfert size */
	drive->prdt[0].transfert_size = nr_sectors * ATA_SECTOR_SIZE;

	/* prepare DMA transfert */
	outb(dma_base, 0);
	outl(dma_base + 0x04, __pa(drive->prdt));
	outb(dma_base + 0x02, inb(dma_base + 0x02) | 0x02 | 0x04);

	/* select sector */
	outb(drive->io_base + ATA_REG_CONTROL, 0x00);
	outb(drive->io_base + ATA_REG_HDDEVSEL, (drive->drive == ATA_MASTER ? 0xE0 : 0xF0) | ((sector >> 24) & 0x0F));
	outb(drive->io_base + ATA_REG_FEATURES, 0x00);
	outb(drive->io_base + ATA_REG_SECCOUNT0, nr_sectors);
	outb(drive->io_base + ATA_REG_LBA0, (uint8_t) sector);
	outb(drive->io_base + ATA_REG_LBA1, (uint8_t) (sector >> 8));
	outb(drive->io_base + ATA_REG_LBA2, (uint8_t) (sector >> 16));

	/* issue read DMA command */
	outb(drive->io_base + ATA_REG_COMMAND, cmd);
	outb(dma_base, (cmd == ATA_CMD_READ_DMA ? 0x08 : 0x00) | 0x01);
}

/*
 * Read from an IDE hd drive.
 */
static int ide_hd_read(struct ide_drive *drive, uint32_t sector, size_t nr_sectors, char *buf)
{
	uint32_t nsect;

	while (nr_sectors) {
		/* limit transfert to buffer size */
		nsect = nr_sectors;
		if (nsect > NR_DMA_SECTORS)
			nsect = NR_DMA_SECTORS;

		/* issue dma command */
		ide_hd_dmaproc(drive, ATA_CMD_READ_DMA, sector, nsect);

		/* wait for completion */
		ide_hd_wait(drive);

		/* copy buffer */
		memcpy(buf, drive->buf, nsect * ATA_SECTOR_SIZE);

		/* update size */
		buf += nsect * ATA_SECTOR_SIZE;
		sector += nsect;
		nr_sectors -= nsect;
	}

	return 0;
}

/*
 * Write to an IDE hd drive.
 */
static int ide_hd_write(struct ide_drive *drive, uint32_t sector, size_t nr_sectors, char *buf)
{
	uint32_t nsect;

	while (nr_sectors) {
		/* limit transfert to buffer size */
		nsect = nr_sectors;
		if (nsect > NR_DMA_SECTORS)
			nsect = NR_DMA_SECTORS;

		/* copy buffer */
		memcpy(drive->buf, buf, nsect * ATA_SECTOR_SIZE);

		/* issue dma command */
		ide_hd_dmaproc(drive, ATA_CMD_WRITE_DMA, sector, nsect);

		/* wait for completion */
		ide_hd_wait(drive);

		/* update size */
		buf += nsect * ATA_SECTOR_SIZE;
		sector += nsect;
		nr_sectors -= nsect;
	}

	return 0;
}

/*
 * Do read/write.
 */
int ide_do_rw_disk(struct ide_drive *drive, struct request *req)
{
	uint32_t start_sector, sector, nr_sectors;

	/* get partition start sector */
	start_sector = drive->part[minor(req->rq_dev) & PARTITION_MINOR_MASK].start_sect;
	sector = start_sector + (req->sector << 9) / ATA_SECTOR_SIZE;
	nr_sectors = (req->nr_sectors << 9) / ATA_SECTOR_SIZE;

	/* read/write */
	switch (req->cmd) {
		case READ:
			return ide_hd_read(drive, sector, nr_sectors, req->buf);
		case WRITE:
			return ide_hd_write(drive, sector, nr_sectors, req->buf);
		default:
			printf("ide_do_rw_disk: can't handle request %x\n", req->cmd);
			return -EIO;
	}
}