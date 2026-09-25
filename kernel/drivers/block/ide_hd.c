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
		status = inb(HWIF(drive)->io_base + ATA_REG_STATUS);
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
 * Do read/write in PIO mode.
 */
static int ide_do_rw_disk_pio(struct ide_drive *drive, struct request *req)
{
	uint32_t sector;
	int ret;

	/* get start sector */
	sector = drive->part[minor(req->rq_dev) & PARTITION_MINOR_MASK].start_sect + req->sector;

	for (; req->nr_sectors > 0; sector++) {
		/* select sector */
		outb(HWIF(drive)->io_base + ATA_REG_CONTROL, 2);
		outb(HWIF(drive)->io_base + ATA_REG_HDDEVSEL, (drive->master ? 0xE0 : 0xF0) | ((sector >> 24) & 0x0F));
		outb(HWIF(drive)->io_base + ATA_REG_FEATURES, 0);
		outb(HWIF(drive)->io_base + ATA_REG_SECCOUNT0, 1);
		outb(HWIF(drive)->io_base + ATA_REG_LBA0, (uint8_t) sector);
		outb(HWIF(drive)->io_base + ATA_REG_LBA1, (uint8_t) (sector >> 8));
		outb(HWIF(drive)->io_base + ATA_REG_LBA2, (uint8_t) (sector >> 16));

		/* issue read/write command */
		outb(HWIF(drive)->io_base + ATA_REG_COMMAND, req->cmd == READ ? ATA_CMD_READ_PIO : ATA_CMD_WRITE_PIO);

		/* wait for disk to be ready */
		ret = ide_hd_wait(drive);
		if (ret)
			return ret;

		/* read or write data */
		if (req->cmd == READ)
			ide_input_data(drive, req);
		else
			ide_output_data(drive, req);

		/* update request */
		req->sector++;
		req->nr_sectors--;
		req->bh_offset += 512;

		/* go to next buffer */
		if (req->bh_offset >= req->bh->b_size) {
			req->bh_offset = 0;
			req->bh = list_next_entry_or_null(req->bh, &req->bhs_list, b_list_req);
		}

		/* wait for drive */
		ret = ide_hd_wait(drive);
		if (ret)
			return ret;
	}

	return 0;
}

/*
 * Do read/write.
 */
int ide_do_rw_disk(struct ide_drive *drive, struct request *req)
{
	int dma_stat, stat;
	uint32_t sector;

	/* compute sector */
	sector = drive->part[minor(req->rq_dev) & PARTITION_MINOR_MASK].start_sect + req->sector;

	/* check command */
	if (req->cmd != READ && req->cmd != WRITE) {
		printf("ide_do_rw_disk: can't handle request %x\n", req->cmd);
		return -EIO;
	}

	/* select sector */
	outb(HWIF(drive)->io_base + ATA_REG_CONTROL, 0);
	outb(HWIF(drive)->io_base + ATA_REG_HDDEVSEL, (drive->master ? 0xE0 : 0xF0) | ((sector >> 24) & 0x0F));
	outb(HWIF(drive)->io_base + ATA_REG_FEATURES, 0);
	outb(HWIF(drive)->io_base + ATA_REG_SECCOUNT0, req->nr_sectors);
	outb(HWIF(drive)->io_base + ATA_REG_LBA0, (uint8_t) sector);
	outb(HWIF(drive)->io_base + ATA_REG_LBA1, (uint8_t) (sector >> 8));
	outb(HWIF(drive)->io_base + ATA_REG_LBA2, (uint8_t) (sector >> 16));

	/* issue dma command */
	if (drive->using_dma && ide_dmaproc(drive, req) == 0) {
		/* wait for completion */
		for (;;) {
			dma_stat = inb(drive->hwif->dma_base + 2);
			stat = inb(HWIF(drive)->io_base + ATA_REG_STATUS);

			if (!(dma_stat & 4))
				continue;

			if (!(stat & ATA_SR_BSY))
				return 0;
		}
	}

	/* on failure try pio mode */
	return ide_do_rw_disk_pio(drive, req);
}