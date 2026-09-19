#include <drivers/block/ide.h>
#include <x86/io.h>
#include <stderr.h>
#include <stdio.h>

/*
 * Read PIO irq handler.
 */
static void ide_hd_read_irq_handler(struct ide_drive *drive)
{
	struct ide_hwgroup *hwgroup = drive->hwif->hwgroup;
	struct request *req;
	uint8_t stat;

	/* check status */
	stat = inb(drive->io_base + ATA_REG_STATUS);
	if (!ATA_OK_STAT(stat, ATA_SR_DRDY, ATA_SR_BSY | ATA_SR_ERR)) {
		printf("ide_hd_read_irq_handler: bad status on drive %s : 0x%x\n", drive->name, stat);
		return;
	}

	/* get request */
	req = hwgroup->req;

	/* read data */
	insw(drive->io_base + ATA_REG_DATA, req->buf, ATA_SECTOR_SIZE / 2);

	/* update request */
	req->sector++;
	req->nr_sectors--;
	req->buf += ATA_SECTOR_SIZE;

	/* end request */
	if (req->nr_sectors <= 0) {
		ide_end_request(hwgroup, 1);
		return;
	}

	/* or continue this request */
	drive->hwif->hwgroup->handler = &ide_hd_read_irq_handler;
}

/*
 * Write PIO irq handler.
 */
static void ide_hd_write_irq_handler(struct ide_drive *drive)
{
	struct ide_hwgroup *hwgroup = drive->hwif->hwgroup;
	struct request *req;
	uint8_t stat;

	/* check status */
	stat = inb(drive->io_base + ATA_REG_STATUS);
	if (!ATA_OK_STAT(stat, ATA_SR_DRDY, ATA_SR_BSY | ATA_SR_ERR | ATA_SR_DF)) {
		printf("ide_hd_write_irq_handler: bad status on drive %s : 0x%x\n", drive->name, stat);
		return;
	}

	/* get request */
	req = hwgroup->req;

	/* update request */
	req->sector++;
	req->nr_sectors--;
	req->buf += ATA_SECTOR_SIZE;

	/* end request */
	if (req->nr_sectors <= 0) {
		ide_end_request(hwgroup, 1);
		return;
	}

	/* write next data */
	drive->hwif->hwgroup->handler = &ide_hd_write_irq_handler;
	outsw(drive->io_base + ATA_REG_DATA, req->buf, ATA_SECTOR_SIZE / 2);
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

	/* check command */
	if (req->cmd != READ && req->cmd != WRITE) {
		printf("ide_do_rw_disk: can't handle request %x\n", req->cmd);
		return -EIO;
	}

	/* select sector */
	outb(drive->io_base + ATA_REG_CONTROL, 0x00);
	outb(drive->io_base + ATA_REG_SECCOUNT0, nr_sectors);
	outb(drive->io_base + ATA_REG_LBA0, (uint8_t) sector);
	outb(drive->io_base + ATA_REG_LBA1, (uint8_t) (sector >> 8));
	outb(drive->io_base + ATA_REG_LBA2, (uint8_t) (sector >> 16));

	/* issue dma command */
	if (ide_dmaproc(drive, req) == 0)
		return 0;

	/* issue read/write pio */
	if (req->cmd == READ) {
		drive->hwif->hwgroup->handler = &ide_hd_read_irq_handler;
		outb(drive->io_base + ATA_REG_COMMAND, ATA_CMD_READ_PIO);
	} else {
		/* issue write */
		outb(drive->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_PIO);
		if (ide_wait_stat(drive, ATA_SR_DRQ, ATA_SR_ERR | ATA_SR_DF, TIMEOUT_WAIT_DRQ)) {
			printf("ide_do_rw_disk: no DRQ on drive %s after issuing write\n", drive->name);
			return -EIO;
		}

		/* write first sector */
		drive->hwif->hwgroup->handler = &ide_hd_write_irq_handler;
		outsw(drive->io_base + ATA_REG_DATA, req->buf, ATA_SECTOR_SIZE / 2);
	}

	return 0;
}
