#include <drivers/block/ide.h>
#include <x86/io.h>
#include <stderr.h>
#include <stdio.h>

#define ORDER_DMA_PAGES		2
#define NR_DMA_PAGES		(1 << (ORDER_DMA_PAGES))
#define NR_DMA_SECTORS		((NR_DMA_PAGES) * PAGE_SIZE / ATA_SECTOR_SIZE)

/*
 * Wait for operation completion.
 */
static void ide_hd_wait(struct ide_drive *drive)
{
	int status, dstatus;

	for (;;) {
		status = inb(drive->bar4 + 2);
		dstatus = inb(drive->io_base + ATA_REG_STATUS);

		if (!(status & 0x04))
			continue;

		if (!(dstatus & ATA_SR_BSY))
			break;
	}
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

		/* set transfert size */
		drive->prdt[0].transfert_size = nsect * ATA_SECTOR_SIZE;

		/* prepare DMA transfert */
		outb(drive->bar4, 0);
		outl(drive->bar4 + 0x04, __pa(drive->prdt));
		outb(drive->bar4 + 0x02, inb(drive->bar4 + 0x02) | 0x02 | 0x04);

		/* select sector */
		outb(drive->io_base + ATA_REG_CONTROL, 0x00);
		outb(drive->io_base + ATA_REG_HDDEVSEL, (drive->drive == ATA_MASTER ? 0xE0 : 0xF0) | ((sector >> 24) & 0x0F));
		outb(drive->io_base + ATA_REG_FEATURES, 0x00);
		outb(drive->io_base + ATA_REG_SECCOUNT0, nsect);
		outb(drive->io_base + ATA_REG_LBA0, (uint8_t) sector);
		outb(drive->io_base + ATA_REG_LBA1, (uint8_t) (sector >> 8));
		outb(drive->io_base + ATA_REG_LBA2, (uint8_t) (sector >> 16));

		/* issue read DMA command */
		outb(drive->io_base + ATA_REG_COMMAND, ATA_CMD_READ_DMA);
		outb(drive->bar4, 0x8 | 0x1);

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

		/* set transfert size */
		drive->prdt[0].transfert_size = nsect * ATA_SECTOR_SIZE;

		/* prepare DMA transfert */
		outb(drive->bar4, 0);
		outl(drive->bar4 + 0x04, __pa(drive->prdt));
		outb(drive->bar4 + 0x02, inb(drive->bar4 + 0x02) | 0x02 | 0x04);

		/* select sector */
		outb(drive->io_base + ATA_REG_CONTROL, 0x00);
		outb(drive->io_base + ATA_REG_HDDEVSEL, (drive->drive == ATA_MASTER ? 0xE0 : 0xF0) | ((sector >> 24) & 0x0F));
		outb(drive->io_base + ATA_REG_FEATURES, 0x00);
		outb(drive->io_base + ATA_REG_SECCOUNT0, nsect);
		outb(drive->io_base + ATA_REG_LBA0, (uint8_t) sector);
		outb(drive->io_base + ATA_REG_LBA1, (uint8_t) (sector >> 8));
		outb(drive->io_base + ATA_REG_LBA2, (uint8_t) (sector >> 16));

		/* issue write DMA command */
		outb(drive->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_DMA);
		outb(drive->bar4, 0x1);

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

/*
 * Init an IDE hd drive.
 */
int ide_hd_init(struct ide_drive *drive)
{
	int ret = -ENOMEM;

	/* allocate prdt */
	drive->prdt = kmalloc(sizeof(struct ata_prdt));
	if (!drive->prdt)
		return -ENOMEM;

	/* allocate buffer */
	drive->buf = get_free_pages(ORDER_DMA_PAGES);
	if (!drive->buf)
		goto err;

	/* clear prdt and buffer */
	memset(drive->prdt, 0, sizeof(struct ata_prdt));
	memset(drive->buf, 0, NR_DMA_PAGES * PAGE_SIZE);

	/* set prdt */
	drive->prdt[0].buffer_phys = __pa(drive->buf);
	drive->prdt[0].mark_end = 0x8000;

	return 0;
err:
	kfree(drive->prdt);
	return ret;
}
