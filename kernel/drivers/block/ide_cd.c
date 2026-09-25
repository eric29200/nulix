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
 * Do read/write in PIO mode.
 */
static int ide_do_rw_cdrom_pio(struct ide_drive *drive, struct request *req)
{
	uint32_t sector, nr_sectors, frame, nr_frames, nskip;
	int ret, len, sectors_to_transfer;
	uint8_t cmd[12] = { 0 };
	char buf[SECTOR_SIZE];

	/* get start sector */
	sector = drive->part[minor(req->rq_dev) & PARTITION_MINOR_MASK].start_sect + req->sector;

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

	for (; nr_frames > 0; nr_frames--) {
		/* select drive */
		outb(HWIF(drive)->io_base + ATA_REG_HDDEVSEL, drive->master ? 0xE0 : 0xF0);
		outb(HWIF(drive)->io_base + ATA_REG_FEATURES, 0);

		/* issue packet command */
		outb(HWIF(drive)->io_base + ATA_REG_LBA1, (uint8_t) (2048 & 0xFF));
		outb(HWIF(drive)->io_base + ATA_REG_LBA2, (uint8_t) (2048 >> 8));
		outb(HWIF(drive)->io_base + ATA_REG_COMMAND, ATA_CMD_PACKET);

		/* wait for completion */
		ret = ide_cd_wait(drive);
		if (ret)
			return ret;

		/* prepare read command */
		cmd[0] = 0xA8;
		cmd[2] = (frame >> 24) & 0xFF;
		cmd[3] = (frame >> 16) & 0xFF;
		cmd[4] = (frame >> 8) & 0xFF;
		cmd[5] = frame & 0xFF;
		cmd[9] = 1;

		/* issue read command */
		outsw(HWIF(drive)->io_base, cmd, 6);

		/* wait for completion */
		ret = ide_cd_wait(drive);
		if (ret)
			return ret;

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
	}

	return 0;
}

/*
 * Do read/write.
 */
int ide_do_rw_cdrom(struct ide_drive *drive, struct request *req)
{
	/* read only */
	if (req->cmd != READ) {
		printf("ide_do_rw_cdrom: can't handle request %x\n", req->cmd);
		return -EIO;
	}

	return ide_do_rw_cdrom_pio(drive, req);
}