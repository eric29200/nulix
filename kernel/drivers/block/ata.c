#include <drivers/block/ata.h>
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
 * Get an ata device.
 */
static struct ata_device *ata_get_device(dev_t dev)
{
	int major = major(dev), h, unit;
	struct ata_device *drive;
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
 * Get partition start sector.
 */
static uint32_t ata_get_start_sector(struct ata_device *device, dev_t dev)
{
	int partition_nr;

	/* get partition number */
	partition_nr = dev - device->hd.dev;
	if (!partition_nr)
		return 0;

	return device->hd.partitions[partition_nr].start_sect;
}

/*
 * Get number of sectors.
 */
static uint32_t ata_get_nr_sectors(struct ata_device *device, dev_t dev)
{
	int partition_nr;

	/* get partition number */
	partition_nr = dev - device->hd.dev;
	if (!partition_nr)
		return 0;

	return device->hd.partitions[partition_nr].nr_sects;
}

/*
 * Handle a read/write request.
 */
static void ata_request(struct ide_hwif *hwif)
{
	uint32_t start_sector, sector, nr_sectors;
	struct ata_device *device;
	struct request *request;
	int ret;

repeat:
	/* get next request */
	request = blk_dev[hwif->major].current_request;
	if (!request)
		return;

	/* remove it from queue */
	blk_dev[hwif->major].current_request = request->next;

	/* get ata device */
	device = ata_get_device(request->rq_dev);
	if (!device) {
		printf("ata_request: can't find device 0x%x\n", request->rq_dev);
		goto next;
	}

	/* get partition start sector */
	start_sector = ata_get_start_sector(device, request->rq_dev);
	sector = start_sector + (request->sector << 9) / device->sector_size;
	nr_sectors = (request->nr_sectors << 9) / device->sector_size;

	/* find request function */
	switch (request->cmd) {
		case READ:
			ret = device->read(device, sector, nr_sectors, request->buf);
			break;
		case WRITE:
			ret = device->write(device, sector, nr_sectors, request->buf);
			break;
		default:
			printf("ata_request: can't handle request %x\n", request->cmd);
			goto next;
	}

	/* print error */
	if (ret)
		printf("ata_request: error on request (cmd = %x, sector = %ld)\n", request->cmd, request->sector);

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
	ata_request(&ide_hwifs[0]);
}

/*
 * Handle a read/write request on interface 1.
 */
static void do_ide1_request()
{
	ata_request(&ide_hwifs[1]);
}

/*
 * Poll for identification.
 */
static int ata_poll_identify(struct ata_device *device)
{
	uint8_t status;
	uint16_t id;

	/* wait until BSY is clear */
	while (1) {
		status = inb(device->io_base + ATA_REG_STATUS);
		if (!status)
			return -ENXIO;

		if (!(status & ATA_SR_BSY))
			break;
	}

	/* check if it is an atapi device */
	id = (inb(device->io_base + ATA_REG_LBA1) << 8) | inb(device->io_base + ATA_REG_LBA2);
	if (id == 0x14EB) {
		device->is_atapi = 1;
		goto out;
	}

	/* wait until DRQ (drive has data to transfer) is clear */
	while (1) {
		status = inb(device->io_base + ATA_REG_STATUS);
		if (status & ATA_SR_ERR)
			return -EFAULT;

		if (status & ATA_SR_DRQ)
			break;
	}

out:
	/* read identified drive data */
	insw(device->io_base + ATA_REG_DATA, &device->identify, 256);

	return 0;
}

/*
 * Detect an ATA device.
 */
static int ata_detect(struct ide_hwif *hwif, struct ata_device *device)
{
	int ret;

	/* select drive */
	outb(device->io_base + ATA_REG_HDDEVSEL, device->drive == ATA_MASTER ? 0xA0 : 0xB0);

	/* identify drive */
	outb(device->io_base + ATA_REG_SECCOUNT0, 0);
	outb(device->io_base + ATA_REG_LBA0, 0);
	outb(device->io_base + ATA_REG_LBA1, 0);
	outb(device->io_base + ATA_REG_LBA2, 0);
	outb(device->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

	/* poll for identification */
	ret = ata_poll_identify(device);
	if (ret)
		return ret;

	/* set gendisk */
	device->hd.dev = mkdev(hwif->major, device->id << PARTITION_MINOR_SHIFT);

	/* init drive */
	if (device->is_atapi)
		ret = ata_cd_init(device);
	else
		ret = ata_hd_init(device);

	/* set device present */
	if (ret == 0)
		device->present = 1;

	return ret;
}

/*
 * Ioctl write.
 */
static int ata_ioctl(struct inode *inode, struct file *filp, int request, unsigned long arg)
{
	struct ata_device *device;
	dev_t dev = inode->i_rdev;

	UNUSED(filp);

	/* get ata device */
	device = ata_get_device(dev);
	if (!device)
		return -EINVAL;

	switch (request) {
		case BLKGETSIZE:
			*((uint32_t *) arg) = ata_get_nr_sectors(device, dev);
			break;
		case BLKGETSIZE64:
			*((uint64_t *) arg) = ata_get_nr_sectors(device, dev) * ATA_SECTOR_SIZE;
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
 * ATA file operations.
 */
static struct file_operations ata_fops = {
	.read		= generic_block_read,
	.write		= generic_block_write,
	.ioctl		= ata_ioctl,
};

/*
 * Probe for drives of an IDE interface.
 */
static void probe_hwif(struct ide_hwif *hwif)
{
	struct ata_device *drive;
	int unit, ret;

	for (unit = 0; unit < MAX_DRIVES; unit++) {
		drive = &hwif->drives[unit];

		/* detect device */
		ret = ata_detect(hwif, drive);
		if (ret)
			continue;

		/* interface present */
		if (!hwif->present)
			hwif->present = 1;
	}
}

/*
 * Init an IDE interface.
 */
static int hwif_init(int h)
{
	struct ide_hwif *hwif = &ide_hwifs[h];
	struct ata_device *drive;
	int ret, i, j;

	/* interface not present */
	if (!hwif->present)
		return 0;

	/* register block device */
	ret = register_blkdev(hwif->major, hwif->name, &ata_fops);
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

	/* init drives */
	for (i = 0; i < MAX_DRIVES; i++) {
		drive = &hwif->drives[i];

		/* drive not present */
		if (!drive->present)
			continue;

		/* discover partitions */
		check_partition(&drive->hd);

		/* set partitions size */
		for (j = 0; j < NR_PARTITIONS; j++)
			blk_size[hwif->major][(i << PARTITION_MINOR_SHIFT) + j] = drive->hd.partitions[j].nr_sects >> (BLOCK_SIZE_BITS - 9);
	}

	return 0;
err_blk_size:
	kfree(blksize_size);
err_blksize_size:
	unregister_blkdev(hwif->major, hwif->name);
	return -ENOMEM;
}

/*
 * Probe a ata device.
 */
static int ata_pci_probe(struct pci_device *pci_dev, struct pci_device_id *id)
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
static struct pci_device_id ata_pci_tbl[] = {
	{ PCI_VENDOR_ID_ATA, PCI_DEVICE_ID_ATA },
	{ 0, }
};

/*
 * PCI driver.
 */
static struct pci_driver ata_pci_driver = {
	.id_table		= ata_pci_tbl,
	.probe			= ata_pci_probe,
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
	struct ata_device *drive;
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
 * Init ata devices.
 */
int init_ata()
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

	return 0;
}
