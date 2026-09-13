#include <drivers/block/ide.h>
#include <drivers/block/blk_dev.h>
#include <drivers/pci/pci.h>
#include <x86/interrupt.h>
#include <x86/io.h>
#include <mm/mm.h>
#include <stderr.h>
#include <stdio.h>
#include <fcntl.h>
#include <dev.h>

/* global variables */
static struct ide_hwif ide_hwifs[MAX_HWIFS] = { 0 };
static uint8_t ide_hwif_to_major[MAX_HWIFS] = { DEV_IDE0_MAJOR, DEV_IDE1_MAJOR };
static uint16_t default_io_base[MAX_HWIFS] = { 0x1F0, 0x170 };

/*
 * Get an IDE drive.
 */
static struct ide_drive *ide_get_drive(dev_t dev)
{
	int major = major(dev), h, unit;
	struct ide_drive *drive;
	struct ide_hwif *hwif;

	for (h = 0; h < MAX_HWIFS; h++) {
		hwif = &ide_hwifs[h];

		if (hwif->present && major == hwif->major) {
			unit = minor(dev) >> PARTITION_MINOR_SHIFT;
			if (unit < MAX_DRIVES) {
				drive = &hwif->drives[unit];
				if (drive->present)
					return drive;
			}

			return NULL;
		}
	}

	return NULL;
}

/*
 * Handle a read/write request.
 */
static void ide_request(struct ide_hwif *hwif)
{
	uint32_t start_sector, sector, nr_sectors;
	struct ide_drive *drive;
	struct request *request;
	int ret;

repeat:
	/* get next request */
	request = blk_dev[hwif->major].current_request;
	if (!request)
		return;

	/* remove it from queue */
	blk_dev[hwif->major].current_request = request->next;

	/* get ide drive */
	drive = ide_get_drive(request->rq_dev);
	if (!drive) {
		printf("ide_request: can't find device 0x%x\n", request->rq_dev);
		goto next;
	}

	/* get partition start sector */
	start_sector = drive->part[minor(request->rq_dev) & PARTITION_MINOR_MASK].start_sect;
	sector = start_sector + (request->sector << 9) / drive->sector_size;
	nr_sectors = (request->nr_sectors << 9) / drive->sector_size;

	/* find request function */
	switch (request->cmd) {
		case READ:
			ret = drive->read(drive, sector, nr_sectors, request->buf);
			break;
		case WRITE:
			ret = drive->write(drive, sector, nr_sectors, request->buf);
			break;
		default:
			printf("ide_request: can't handle request %x\n", request->cmd);
			goto next;
	}

	/* print error */
	if (ret)
		printf("ide_request: error on request (cmd = %x, sector = %ld)\n", request->cmd, request->sector);

next:
	/* end this request */
	end_request(request);
	goto repeat;
}

/*
 * Handle a read/write request on interface 0.
 */
static void do_ide0_request()
{
	ide_request(&ide_hwifs[0]);
}

/*
 * Handle a read/write request on interface 1.
 */
static void do_ide1_request()
{
	ide_request(&ide_hwifs[1]);
}

/*
 * Poll for identification.
 */
static int ide_poll_identify(struct ide_drive *drive)
{
	uint8_t status;
	uint16_t id;

	/* wait until BSY is clear */
	while (1) {
		status = inb(drive->io_base + ATA_REG_STATUS);
		if (!status)
			return -ENXIO;

		if (!(status & ATA_SR_BSY))
			break;
	}

	/* check if it is an atapi drive */
	id = (inb(drive->io_base + ATA_REG_LBA1) << 8) | inb(drive->io_base + ATA_REG_LBA2);
	if (id == 0x14EB) {
		drive->is_atapi = 1;
		goto out;
	}

	/* wait until DRQ (drive has data to transfer) is clear */
	while (1) {
		status = inb(drive->io_base + ATA_REG_STATUS);
		if (status & ATA_SR_ERR)
			return -EFAULT;

		if (status & ATA_SR_DRQ)
			break;
	}

out:
	/* read identified drive data */
	insw(drive->io_base + ATA_REG_DATA, &drive->identify, 256);

	return 0;
}

/*
 * Identify an IDE drive.
 */
static int ide_identify(struct ide_drive *drive)
{
	int ret;

	/* select drive */
	outb(drive->io_base + ATA_REG_HDDEVSEL, drive->drive == ATA_MASTER ? 0xA0 : 0xB0);

	/* identify drive */
	outb(drive->io_base + ATA_REG_SECCOUNT0, 0);
	outb(drive->io_base + ATA_REG_LBA0, 0);
	outb(drive->io_base + ATA_REG_LBA1, 0);
	outb(drive->io_base + ATA_REG_LBA2, 0);
	outb(drive->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

	/* poll for identification */
	ret = ide_poll_identify(drive);
	if (ret)
		return ret;

	/* init drive */
	if (drive->is_atapi)
		ret = ide_cd_init(drive);
	else
		ret = ide_hd_init(drive);

	/* set device present */
	if (ret == 0)
		drive->present = 1;

	return ret;
}

/*
 * Ioctl write.
 */
static int ide_ioctl(struct inode *inode, struct file *filp, int request, unsigned long arg)
{
	dev_t dev = inode->i_rdev;
	struct ide_drive *drive;

	UNUSED(filp);

	/* get ide drive */
	drive = ide_get_drive(dev);
	if (!drive)
		return -EINVAL;

	switch (request) {
		case BLKGETSIZE:
			*((uint32_t *) arg) = drive->part[minor(dev) & PARTITION_MINOR_MASK].nr_sects;
			break;
		case BLKGETSIZE64:
			*((uint64_t *) arg) = drive->part[minor(dev) & PARTITION_MINOR_MASK].nr_sects * ATA_SECTOR_SIZE;
			break;
		case BLKSSZGET:
		 	*((uint32_t *) arg) = blksize_size[major(dev)][minor(dev)];
			break;
		case BLKROGET:
		 	*((int *) arg) = is_read_only(dev);
			break;
		case BLKDISCARDZEROES:
			break;
		default:
			printf("Unknown ioctl request (0x%x) on device 0x%x\n", request, (int) dev);
			break;
	}

	return 0;
}

/*
 * IDE file operations.
 */
static struct file_operations ide_fops = {
	.read		= generic_block_read,
	.write		= generic_block_write,
	.ioctl		= ide_ioctl,
};

/*
 * Probe for drives of an IDE interface.
 */
static void probe_hwif(struct ide_hwif *hwif)
{
	struct ide_drive *drive;
	int unit, ret;

	for (unit = 0; unit < MAX_DRIVES; unit++) {
		drive = &hwif->drives[unit];

		/* identify device */
		ret = ide_identify(drive);
		if (ret)
			continue;

		/* interface present */
		if (!hwif->present)
			hwif->present = 1;
	}
}

/*
 * Init gendisk.
 */
static void init_gendisk(struct ide_hwif *hwif)
{
	int units, minors, unit;
	struct gendisk *gd;
	size_t *bs;

	/* figure out maximum drive number on the interface */
	for (units = MAX_DRIVES; units > 0; units--) {
		if (hwif->drives[units - 1].present)
			break;
	}

	/* compute number of minors */
	minors = units * NR_PARTITIONS;

	/* allocate gendisk structure */
	gd = kmalloc(sizeof(struct gendisk));
	if (!gd)
		goto err_kmalloc_gd;
	memset(gd, 0, sizeof(struct gendisk));

	/* allocate arrays */
	gd->sizes = kmalloc(minors * sizeof(size_t));
	if (!gd->sizes)
		goto err_kmalloc_gd_sizes;
	gd->part = kmalloc(minors * sizeof(struct partition));
	if (!gd->part)
		goto err_kmalloc_gd_part;
	bs = kmalloc(minors * sizeof(size_t));
	if (!bs)
		goto err_kmalloc_bs;

	/* clear partitions */
	memset(gd->part, 0, minors * sizeof(struct partition));

	/* set default block size */
	blksize_size[hwif->major] = bs;
	for (unit = 0; unit < minors; unit++)
		*bs++ = BLOCK_SIZE;

	/* set partitions on each drive */
	for (unit = 0; unit < units; ++unit)
		hwif->drives[unit].part = &gd->part[unit << PARTITION_MINOR_SHIFT];

	/* init gendisk */
	gd->major = hwif->major;
	gd->minor_shift	= PARTITION_MINOR_SHIFT;
	gd->max_p = NR_PARTITIONS;
	gd->nr_real = units;

	/* add gendisk */
	hwif->gd = gd;
	add_gendisk(gd);

	return;
err_kmalloc_bs:
	kfree(gd->part);
err_kmalloc_gd_part:
	kfree(gd->sizes);
err_kmalloc_gd_sizes:
	kfree(gd);
err_kmalloc_gd:
	panic("init_gendisk: Out of memory\n");
	return;
}


/*
 * Init an IDE interface.
 */
static int hwif_init(int h)
{
	struct ide_hwif *hwif = &ide_hwifs[h];
	int ret, i;

	/* interface not present */
	if (!hwif->present)
		return 0;

	/* register block device */
	ret = register_blkdev(hwif->major, hwif->name, &ide_fops);
	if (ret)
		return ret;

	/* allocate block size array */
	blksize_size[hwif->major] = kmalloc(MAX_DRIVES * NR_PARTITIONS * sizeof(size_t));
	if (!blksize_size[hwif->major])
		goto err_blksize_size;

	/* allocate size array */
	blk_size[hwif->major] = kmalloc(MAX_DRIVES * NR_PARTITIONS * sizeof(size_t));
	if (!blk_size[hwif->major])
		goto err_blk_size;

	/* set default block size */
	for (i = 0; i < MAX_DRIVES * NR_PARTITIONS; i++)
		blksize_size[hwif->major][i] = BLOCK_SIZE;

	/* register block device */
	switch (hwif->major) {
		case DEV_IDE0_MAJOR:
			blk_dev[hwif->major].request = do_ide0_request;
			break;
		case DEV_IDE1_MAJOR:
			blk_dev[hwif->major].request = do_ide1_request;
			break;
	}

	/* init gendisk */
	init_gendisk(hwif);

	return 0;
err_blk_size:
	kfree(blksize_size);
err_blksize_size:
	unregister_blkdev(hwif->major, hwif->name);
	return -ENOMEM;
}

/*
 * Probe a PCI IDE device.
 */
static int ide_pci_probe(struct pci_device *pci_dev, struct pci_device_id *id)
{
	uint32_t bar4;
	int i, j;

	/* unused device id */
	UNUSED(id);

	/* enable pci device */
	pci_enable_device(pci_dev);
	pci_set_master(pci_dev);

	/* get BAR4 from pci device */
	bar4 = pci_read_field(pci_dev->address, PCI_BAR4);
	if (bar4 & 0x00000001)
		bar4 &= 0xFFFFFFFC;

	/* set BAR4 */
	for (i = 0; i < MAX_HWIFS; i++)
		for (j = 0; j < MAX_DRIVES; j++)
			ide_hwifs[i].drives[j].bar4 = bar4;

	return 0;
}

/*
 * PCI ids table.
 */
static struct pci_device_id ide_pci_tbl[] = {
	{ PCI_VENDOR_ID_ATA, PCI_DEVICE_ID_ATA },
	{ 0, }
};

/*
 * PCI driver.
 */
static struct pci_driver ata_pci_driver = {
	.id_table		= ide_pci_tbl,
	.probe			= ide_pci_probe,
};

/*
 * Probe for IDE interfaces.
 */
static int probe_for_hwifs()
{
	int ret;

	/* register pci driver */
	ret = pci_register_driver(&ata_pci_driver);
	if (ret > 0)
		return 0;

	return ret == 0 ? -ENODEV : 0;
}

/*
 * Init an IDE interface.
 */
static void init_hwif_data(int index)
{
	struct ide_hwif *hwif = &ide_hwifs[index];
	struct ide_drive *drive;
	int unit;

	/* init interface */
	hwif->index = index;
	hwif->major = ide_hwif_to_major[index];
	hwif->name[0] = 'i';
	hwif->name[1] = 'd';
	hwif->name[2] = 'e';
	hwif->name[3] = '0' + index;

	/* init drives */
	for (unit = 0; unit < MAX_DRIVES; unit++) {
		drive = &hwif->drives[unit];
		drive->id = unit;
		drive->drive = unit == 0 ? ATA_MASTER : ATA_SLAVE;
		drive->io_base = default_io_base[index];
		drive->name[0] = 'h';
		drive->name[1] = 'd';
		drive->name[2] = 'a' + (index * MAX_DRIVES) + unit;
	}
}

/*
 * Init IDE devices.
 */
int init_ide()
{
	int ret, i;

	/* init interfaces */
	for (i = 0; i < MAX_HWIFS; i++)
		init_hwif_data(i);

	/* probe for interfaces */
	ret = probe_for_hwifs();
	if (ret)
		return ret;

	/* probe for drives */
	for (i = 0; i < MAX_HWIFS; i++)
		probe_hwif(&ide_hwifs[i]);

	/* final init interfaces */
	for (i = 0; i < MAX_HWIFS; i++)
		hwif_init(i);

	/* setup gendisk */
	setup_gendisk();

	return 0;
}
