#include <drivers/block/ide.h>
#include <mm/highmem.h>
#include <x86/io.h>
#include <stderr.h>
#include <stdio.h>
#include <dev.h>

#define CD_FRAMESIZE		2048
#define SECTOR_SIZE		512
#define SECTOR_BITS		9
#define SECTORS_PER_FRAME	(CD_FRAMESIZE / SECTOR_SIZE)

/*
 * Read irq handler.
 */
static void ide_cd_read_irq_handler(struct ide_drive *drive)
{
	struct ide_hwgroup *hwgroup = drive->hwif->hwgroup;
	size_t len, sectors_to_transfer, nskip;
	struct request *req = hwgroup->req;
	char buf[SECTOR_SIZE];
	uint8_t stat;

	/* check status */
	stat = inb(drive->io_base + ATA_REG_STATUS);
	if (!ATA_OK_STAT(stat, ATA_SR_DRDY, ATA_SR_BSY | ATA_SR_ERR)) {
		printf("ide_cd_read_irq_handler: bad status on drive %s : 0x%x\n", drive->name, stat);
		return;
	}

	/* read the interrupt reason and the transfer length */
	inb(drive->io_base + ATA_REG_SECCOUNT0);
	len = inb(drive->io_base + ATA_REG_LBA1) + 256 * inb(drive->io_base + ATA_REG_LBA2);

	/* if DRQ is clear, the command has completed */
	if ((stat & ATA_SR_DRQ) == 0) {
		if (req->current_nr_sectors > 0) {
			printf("ide_cd_read_irq_handler: data underrun on drive %s (%ld blocks)\n", drive->name, req->nr_sectors);
			ide_end_request(hwgroup, 0);
		} else {
			ide_end_request(hwgroup, 1);
		}

		return;
	}

	/* get number of sectors to transfer */
	sectors_to_transfer = len / 512;

	/* skip first sectors if needed */
	nskip = req->current_nr_sectors - req->nr_sectors;
	while (nskip > 0) {
		ide_input_data_buf(drive, buf, SECTOR_SIZE);
		req->current_nr_sectors--;
		nskip--;
		sectors_to_transfer--;
	}

	/* read sectors */
	while (sectors_to_transfer > 0) {
		/* request done : throw away remaining sectors */
		if (req->current_nr_sectors == 0) {
			ide_input_data_buf(drive, buf, SECTOR_SIZE);
			sectors_to_transfer--;
			continue;
		}

		/* read data */
		ide_input_data(drive, req);

		/* update request */
		req->sector++;
		req->nr_sectors--;
		req->current_nr_sectors--;
		req->bh_offset += SECTOR_SIZE;
		sectors_to_transfer--;

		/* go to next buffer */
		if (req->bh_offset >= req->bh->b_size) {
			req->bh_offset = 0;
			req->bh = list_next_entry_or_null(req->bh, &req->bhs_list, b_list_req);
		}
	}

	/* continue request */
	drive->hwif->hwgroup->handler = &ide_cd_read_irq_handler;
}

/*
 * Continue sending a read request.
 */
static void ide_cd_start_read_continuation(struct ide_drive *drive)
{
	uint32_t sector, nr_sectors, frame, nr_frames, nskip;
	struct request *req = drive->hwif->hwgroup->req;
	uint8_t cmd[12] = { 0 };

	/* get sector */
	sector = drive->part[minor(req->rq_dev) & PARTITION_MINOR_MASK].start_sect + req->sector;
	nr_sectors = req->nr_sectors;

	/* request must start on a cdrom block boundary */
	nskip = sector % SECTORS_PER_FRAME;
	if (nskip > 0) {
		sector -= nskip;
		nr_sectors += nskip;
		req->current_nr_sectors += nskip;
	}

	/* get frame */
	frame = sector / SECTORS_PER_FRAME;
	nr_frames = (nr_sectors + SECTORS_PER_FRAME - 1) / SECTORS_PER_FRAME;

	/* limit to 64k - 1 */
	if (nr_frames > 65535)
		nr_frames = 65535;

	/* prepare read command */
	cmd[0] = 0x28;
	cmd[2] = (frame >> 24) & 0xFF;
	cmd[3] = (frame >> 16) & 0xFF;
	cmd[4] = (frame >> 8) & 0xFF;
	cmd[5] = frame & 0xFF;
	cmd[7] = nr_frames >> 8;
	cmd[8] = nr_frames & 0xFF;

	/* wait for DRQ */
	if (ide_wait_stat(drive, ATA_SR_DRQ, ATA_SR_BSY, TIMEOUT_WAIT_READY))
		return;

	/* issue read command */
	drive->hwif->hwgroup->handler = &ide_cd_read_irq_handler;
	outsw(drive->io_base, cmd, 6);
}

/*
 * Start sending a read request.
 */
static int ide_cd_start_packet_command(struct ide_drive *drive, int xferlen)
{
	/* wait for the drive */
	if (ide_wait_stat(drive, 0, ATA_SR_BSY, TIMEOUT_WAIT_READY))
		return -EIO;

	/* setup registers */
	outb(drive->io_base + ATA_REG_FEATURES, 0);
	outb(drive->io_base + ATA_REG_SECCOUNT0, 0);
	outb(drive->io_base + ATA_REG_LBA0, 0);
	outb(drive->io_base + ATA_REG_LBA1, xferlen & 0xFF);
	outb(drive->io_base + ATA_REG_LBA2, xferlen >> 8);

	/* issue packet command */
	outb(drive->io_base + ATA_REG_COMMAND, ATA_CMD_PACKET);

	/* continue read = send read command */
	ide_cd_start_read_continuation(drive);

	return 0;
}

/*
 * Do read/write.
 */
int ide_do_rw_cdrom(struct ide_drive *drive, struct request *req, uint32_t block)
{
	int minor = minor(req->rq_dev);

	/* read only  */
	if (req->cmd != READ) {
		printf("ide_do_rw_cdrom: can't handle request %x\n", req->cmd);
		return -EIO;
	}

	/* if the request is relative to a partition, fix it up to refer to the absolute address */
	if ((minor & PARTITION_MINOR_MASK) != 0) {
		req->sector = block;
		minor &= ~PARTITION_MINOR_MASK;
		req->rq_dev = mkdev(major(req->rq_dev), minor);
	}

	/* start sending the request */
	return ide_cd_start_packet_command(drive, 32768);
}