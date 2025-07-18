#include <drivers/block/ata.h>
#include <drivers/block/blk_dev.h>
#include <x86/interrupt.h>
#include <x86/io.h>
#include <mm/mm.h>
#include <stderr.h>
#include <stdio.h>
#include <fcntl.h>
#include <dev.h>

/* ata devices */
static struct ata_device ata_devices[NR_ATA_DEVICES] = {
	{
		.id 		= 0,
		.bus 		= ATA_PRIMARY,
		.drive 		= ATA_MASTER,
		.io_base 	= ATA_PRIMARY_IO
	},
	{
		.id 		= 1,
		.bus 		= ATA_PRIMARY,
		.drive 		= ATA_SLAVE,
		.io_base 	= ATA_PRIMARY_IO
	},
	{
		.id 		= 2,
		.bus 		= ATA_SECONDARY,
		.drive 		= ATA_MASTER,
		.io_base 	= ATA_SECONDARY_IO
	},
	{
		.id 		= 3,
		.bus 		= ATA_SECONDARY,
		.drive 		= ATA_SLAVE,
		.io_base 	= ATA_SECONDARY_IO
	},
};

/* ata block sizes */
static size_t ata_blocksizes[NR_ATA_DEVICES * NR_PARTITIONS] = { 0, };

/*
 * Get an ata device.
 */
static struct ata_device *ata_get_device(dev_t dev)
{
	int id;

	/* check major number */
	if (major(dev) != DEV_ATA_MAJOR)
		return NULL;

	/* get device id */
	id = minor(dev) >> PARTITION_MINOR_SHIFT;
	if (id >= NR_ATA_DEVICES)
		return NULL;

	return &ata_devices[id];
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
static void ata_request(struct request *request)
{
	uint32_t start_sector, sector, nr_sectors;
	struct ata_device *device;
	int ret;

	/* get ata device */
	device = ata_get_device(request->dev);
	if (!device) {
		printf("ata_request: can't find device 0x%x\n", request->dev);
		return;
	}

	/* get partition start sector */
	start_sector = ata_get_start_sector(device, request->dev);
	sector = start_sector + request->block * request->block_size / device->sector_size;
	nr_sectors = request->size / device->sector_size;

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
			return;
	}

	/* print error */
	if (ret)
		printf("ata_request: error on request (cmd = %x, block = %ld)\n", request->cmd, request->block);
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
static int ata_detect(struct ata_device *device)
{
	int ret;

	/* select drive */
	outb(device->io_base + ATA_REG_HDDEVSEL, device->drive == ATA_MASTER ? 0xA0 : 0xB0);

	/* identify drive */
	outb(device->io_base + ATA_REG_SECCOUNT0, 0);
	outb(device->io_base + ATA_REG_LBA0, 0);
	outb(device->io_base + ATA_REG_LBA2, 0);
	outb(device->io_base + ATA_REG_LBA0, 0);
	outb(device->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

	/* poll for identification */
	ret = ata_poll_identify(device);
	if (ret)
		return ret;

	/* set gendisk */
	device->hd.dev = mkdev(DEV_ATA_MAJOR, device->id << PARTITION_MINOR_SHIFT);
	sprintf(device->hd.name, "hd%c", 'a' + device->id);

	/* init drive */
	if (device->is_atapi)
		ret = ata_cd_init(device);
	else
		ret = ata_hd_init(device);

	return ret;
}

/*
 * Ioctl write.
 */
static int ata_ioctl(struct file *filp, int request, unsigned long arg)
{
	dev_t dev = filp->f_inode->i_rdev;
	struct ata_device *device;

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
		default:
			printf("Unknown ioctl request (0x%x) on device 0x%x\n", request, (int) filp->f_inode->i_rdev);
			break;
	}

	return 0;
}

/*
 * Irq handler.
 */
static void ata_irq_handler(struct registers *regs)
{
	UNUSED(regs);
}

/*
 * Init ata devices.
 */
int init_ata()
{
	int ret, i;

	/* register interrupt handlers */
	register_interrupt_handler(IRQ14, ata_irq_handler);
	register_interrupt_handler(IRQ15, ata_irq_handler);

	/* set default block size */
	blocksize_size[DEV_ATA_MAJOR] = ata_blocksizes;

	/* register block device */
	blk_dev[DEV_ATA_MAJOR].request = ata_request;

	/* detect hard drives */
	for (i = 0; i < NR_ATA_DEVICES; i++) {
		ret = ata_detect(&ata_devices[i]);
		if (ret)
			continue;

		/* set default block size */
		ata_blocksizes[i << PARTITION_MINOR_SHIFT] = DEFAULT_BLOCK_SIZE;

		/* discover partitions */
		check_partition(&ata_devices[i].hd);
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
 * ATA inode operations.
 */
struct inode_operations ata_iops = {
	.fops		= &ata_fops,
};
