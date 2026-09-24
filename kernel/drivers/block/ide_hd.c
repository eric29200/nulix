#include <drivers/block/ide.h>
#include <mm/highmem.h>
#include <x86/io.h>
#include <stderr.h>
#include <stdio.h>

static void ide_hd_read_irq_handler(struct ide_drive *drive);
static void ide_hd_write_irq_handler(struct ide_drive *drive);

/*
 * Read PIO irq handler.
 */
static void ide_hd_read_irq_handler(struct ide_drive *drive)
{
	struct ide_hwgroup *hwgroup = HWGROUP(drive);
	struct request *req;
	uint8_t stat;

	/* check status */
	stat = inb(HWIF(drive)->io_base + ATA_REG_STATUS);
	if (!ATA_OK_STAT(stat, ATA_SR_DRDY, ATA_SR_BSY | ATA_SR_ERR)) {
		printf("ide_hd_read_irq_handler: bad status on drive %s : 0x%x\n", drive->name, stat);
		return;
	}

	/* get request */
	req = hwgroup->req;

	/* read data */
	ide_input_data(drive, req);

	/* update request */
	req->sector++;
	req->nr_sectors--;
	req->bh_offset += 512;

	/* go to next buffer */
	if (req->bh_offset >= req->bh->b_size) {
		req->bh_offset = 0;
		req->bh = list_next_entry_or_null(req->bh, &req->bhs_list, b_list_req);
	}

	/* end request */
	if (req->nr_sectors <= 0) {
		ide_end_request(hwgroup, 1);
		return;
	}

	/* or continue request */
	drive->hwif->hwgroup->handler = &ide_hd_read_irq_handler;
}

/*
 * Write PIO irq handler.
 */
static void ide_hd_write_irq_handler(struct ide_drive *drive)
{
	struct ide_hwgroup *hwgroup = HWGROUP(drive);
	struct request *req;
	uint8_t stat;

	/* check status */
	stat = inb(HWIF(drive)->io_base + ATA_REG_STATUS);
	if (!ATA_OK_STAT(stat, ATA_SR_DRDY, ATA_SR_BSY | ATA_SR_ERR | ATA_SR_DF)) {
		printf("ide_hd_write_irq_handler: bad status on drive %s : 0x%x\n", drive->name, stat);
		return;
	}

	/* get request */
	req = hwgroup->req;

	/* update request */
	req->sector++;
	req->nr_sectors--;
	req->bh_offset += 512;

	/* go to next buffer */
	if (req->bh_offset >= req->bh->b_size) {
		req->bh_offset = 0;
		req->bh = list_next_entry_or_null(req->bh, &req->bhs_list, b_list_req);
	}

	/* end request */
	if (req->nr_sectors <= 0) {
		ide_end_request(hwgroup, 1);
		return;
	}

	/* or continue request = write next data */
	drive->hwif->hwgroup->handler = &ide_hd_write_irq_handler;
	ide_output_data(drive, req);
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
	outb(HWIF(drive)->io_base + ATA_REG_SECCOUNT0, req->nr_sectors);
	outb(HWIF(drive)->io_base + ATA_REG_LBA0, (uint8_t) block);
	outb(HWIF(drive)->io_base + ATA_REG_LBA1, (uint8_t) (block >> 8));
	outb(HWIF(drive)->io_base + ATA_REG_LBA2, (uint8_t) (block >> 16));

	/* read request */
	if (req->cmd == READ) {
		/* try dma first */
		if (drive->using_dma && ide_dmaproc(drive, req, ide_dma_read) == 0)
			return 0;

		/* or use pio mode */
		drive->hwif->hwgroup->handler = &ide_hd_read_irq_handler;
		outb(HWIF(drive)->io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
		return 0;
	}

	/* write request : try dma first */
	if (drive->using_dma && ide_dmaproc(drive, req, ide_dma_write) == 0)
		return 0;

	/* or use pio mode */
	outb(HWIF(drive)->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
	if (ide_wait_stat(drive, ATA_SR_DRQ, ATA_SR_ERR | ATA_SR_DF, TIMEOUT_WAIT_DRQ)) {
		printf("ide_pio_read: no DRQ on drive %s after issuing write\n", drive->name);
		return -EIO;
	}

	/* write first sector */
	drive->hwif->hwgroup->handler = &ide_hd_write_irq_handler;
	ide_output_data(drive, req);

	return 0;
}
