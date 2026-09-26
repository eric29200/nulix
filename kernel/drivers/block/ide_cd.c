#include <drivers/block/ide.h>
#include <x86/io.h>
#include <stderr.h>
#include <stdio.h>
#include <dev.h>

#define CD_FRAMESIZE		2048
#define SECTOR_SIZE		512
#define SECTOR_BITS		9
#define SECTORS_PER_FRAME	(CD_FRAMESIZE / SECTOR_SIZE)

/*
 * Wait for operation completion.
 */
static int ide_cd_wait(struct ide_drive *drive)
{
	uint8_t status;

	for (;;) {
		status = inb(HWIF(drive)->io_base + ATA_REG_STATUS);
		if (!status)
			return -ENXIO;
		if (status & ATA_SR_ERR)
			return -EIO;
		if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ))
			break;
	}

	return 0;
}

/*
 * End a read request.
 */
static int ide_end_read(struct ide_drive *drive, struct request *req)
{
	int len, sectors_to_transfer;
	char buf[SECTOR_SIZE];
	uint32_t nskip;

	/* read transfer length */
	len = inb(HWIF(drive)->io_base + ATA_REG_LBA1) + 256 * inb(HWIF(drive)->io_base + ATA_REG_LBA2);

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

	return 0;
}

/*
 * Continue sending a read request.
 */
static int ide_start_read_continuation(struct ide_drive *drive, struct request *req)
{
	uint32_t sector, nr_sectors, nskip, frame, nr_frames;
	uint8_t cmd[12] = { 0 };
	int ret;

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

	/* issue read command */
	outsw(HWIF(drive)->io_base, cmd, 6);

	/* wait for completion */
	ret = ide_cd_wait(drive);
	if (ret)
		return ret;

	/* end read = get results */
	return ide_end_read(drive, req);
}

/*
 * Start sending a read request.
 */
static int ide_start_packet_command(struct ide_drive *drive, int xferlen, struct request *req)
{
	int ret;

	/* issue packet command */
	outb(HWIF(drive)->io_base + ATA_REG_FEATURES, 0);
	outb(HWIF(drive)->io_base + ATA_REG_LBA1, xferlen & 0xFF);
	outb(HWIF(drive)->io_base + ATA_REG_LBA2, xferlen >> 8);
	outb(HWIF(drive)->io_base + ATA_REG_COMMAND, ATA_CMD_PACKET);

	/* wait for completion */
	ret = ide_cd_wait(drive);
	if (ret)
		return ret;

	/* continue read */
	return ide_start_read_continuation(drive, req);
}

/*
 * Do read/write.
 */
int ide_do_rw_cdrom(struct ide_drive *drive, struct request *req, uint32_t block)
{
	int minor = minor(req->rq_dev);

	/* read only */
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

	return ide_start_packet_command(drive, 32768, req);
}