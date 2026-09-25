#include <drivers/block/ide.h>
#include <x86/io.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Wait for completion.
 */
static int ide_hd_wait(struct ide_drive *drive, int dma)
{
	uint8_t stat, dma_stat;

	for (;;) {
		stat = inb(HWIF(drive)->io_base + ATA_REG_STATUS);
		dma_stat = dma ? inb(drive->hwif->dma_base + 2) : 0;

		if (!stat)
			return -ENXIO;
		if (stat & ATA_SR_ERR)
			return -EIO;
		if (!(stat & ATA_SR_BSY) && (!dma || (dma_stat & 4)))
			break;
	}

	return 0;
}

/*
 * Start a read/write sector request.
 */
static int ide_start_rw_disk_sector(struct ide_drive *drive, uint32_t sector, int cmd)
{
	int ret;

	/* wait for drive */
	ret = ide_hd_wait(drive, 0);
	if (ret)
		return ret;

	/* select sector */
	outb(HWIF(drive)->io_base + ATA_REG_CONTROL, 2);
	outb(HWIF(drive)->io_base + ATA_REG_HDDEVSEL, (drive->master ? 0xE0 : 0xF0) | ((sector >> 24) & 0x0F));
	outb(HWIF(drive)->io_base + ATA_REG_FEATURES, 0);
	outb(HWIF(drive)->io_base + ATA_REG_SECCOUNT0, 1);
	outb(HWIF(drive)->io_base + ATA_REG_LBA0, (uint8_t) sector);
	outb(HWIF(drive)->io_base + ATA_REG_LBA1, (uint8_t) (sector >> 8));
	outb(HWIF(drive)->io_base + ATA_REG_LBA2, (uint8_t) (sector >> 16));

	/* issue read/write command */
	outb(HWIF(drive)->io_base + ATA_REG_COMMAND, cmd);

	/* wait for disk to be ready */
	return ide_hd_wait(drive, 0);
}

/*
 * Do read/write in PIO mode.
 */
static int ide_do_rw_disk_pio(struct ide_drive *drive, struct request *req, uint32_t sector)
{
	int ret;

	while (req->nr_sectors > 0) {
		/* start request */
		ret = ide_start_rw_disk_sector(drive, sector++, req->cmd == READ ? ATA_CMD_READ_PIO : ATA_CMD_WRITE_PIO);
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
	}

	return 0;
}

/*
 * Do read/write.
 */
int ide_do_rw_disk(struct ide_drive *drive, struct request *req, uint32_t block)
{
	/* check command */
	if (req->cmd != READ && req->cmd != WRITE) {
		printf("ide_do_rw_disk: can't handle request %x\n", req->cmd);
		return -EIO;
	}

	/* select sector */
	outb(HWIF(drive)->io_base + ATA_REG_CONTROL, 0);
	outb(HWIF(drive)->io_base + ATA_REG_HDDEVSEL, (drive->master ? 0xE0 : 0xF0) | ((block >> 24) & 0x0F));
	outb(HWIF(drive)->io_base + ATA_REG_FEATURES, 0);
	outb(HWIF(drive)->io_base + ATA_REG_SECCOUNT0, req->nr_sectors);
	outb(HWIF(drive)->io_base + ATA_REG_LBA0, (uint8_t) block);
	outb(HWIF(drive)->io_base + ATA_REG_LBA1, (uint8_t) (block >> 8));
	outb(HWIF(drive)->io_base + ATA_REG_LBA2, (uint8_t) (block >> 16));

	/* issue dma command */
	if (drive->using_dma && ide_dmaproc(drive, req) == 0)
		return ide_hd_wait(drive, 1);

	/* on failure try pio mode */
	return ide_do_rw_disk_pio(drive, req, block);
}