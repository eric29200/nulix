#include <drivers/block/ide.h>
#include <x86/io.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Do read/write in PIO mode.
 */
static int ide_do_rw_disk_pio(struct ide_drive *drive, struct request *req)
{
	/* issue read/write command */
	outb(HWIF(drive)->io_base + ATA_REG_COMMAND, req->cmd == READ ? ATA_CMD_READ_PIO : ATA_CMD_WRITE_PIO);

	while (req->nr_sectors > 0) {
		/* wait for drive */
		if (ide_wait_stat(drive, ATA_SR_DRQ, ATA_SR_BSY | ATA_SR_ERR, 0)) {
			printf("ide_do_rw_disk_pio: no DRQ on drive %s after issuing read/write\n", drive->name);
			return -EIO;
		}

		/* read or write data */
		if (req->cmd == READ)
			ide_input_data(drive, req);
		else
			ide_output_data(drive, req);

		/* update request */
		req->sector++;
		req->nr_sectors--;
		req->current_nr_sectors--;
		req->bh_offset += 512;

		/* go to next buffer */
		if (req->bh_offset >= req->bh->b_size) {
			req->bh_offset = 0;
			req->bh = list_next_entry_or_null(req->bh, &req->bhs_list, b_list_req);
		}
	}

	/* end request */
	if (req->current_nr_sectors == 0)
		end_request(req, 1);

	return 0;
}

/*
 * Do read/write in dma mode.
 */
static int ide_do_rw_disk_dma(struct ide_drive *drive, struct request *req)
{
	int ret;

	/* drive not using dma */
	if (!drive->using_dma)
		return 1;

	/* start dma transfer */
	ret = ide_dmaproc(drive, req, req->cmd == READ ? ide_dma_read : ide_dma_write);
	if (ret)
		return ret;

	/* wait for drive */
	ret = ide_wait_stat(drive, ATA_SR_DRDY, ATA_SR_BSY | ATA_SR_ERR, 1);
	if (ret)
		printf("ide_do_rw_disk_dma: drive %s on error after issuing read/write\n", drive->name);

	/* end dma */
	ret |= ide_dmaproc(drive, req, ide_dma_end);

	/* end request on success */
	if (ret == 0)
		end_request(req, 1);

	return ret;
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

	/* try dma first */
	if (ide_do_rw_disk_dma(drive, req) == 0)
		return 0;

	/* on failure try pio mode */
	return ide_do_rw_disk_pio(drive, req);
}