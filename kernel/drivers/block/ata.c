#include <drivers/block/ata.h>
#include <fs/dev_fs.h>
#include <x86/interrupt.h>
#include <x86/io.h>
#include <mm/mm.h>
#include <stderr.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <dev.h>

/* ata devices */
static struct ata_device_t ata_devices[NR_ATA_DEVICES] = {
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
static struct ata_device_t *ata_get_device(dev_t dev)
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
static uint32_t ata_get_start_sector(struct ata_device_t *device, dev_t dev)
{
	int partition_nr;

	/* get partition number */
	partition_nr = dev - device->hd.dev;
	if (!partition_nr)
		return 0;

	return device->hd.partitions[partition_nr].start_sect;
}

/*
 * Read from an ata device.
 */
int ata_read(dev_t dev, struct buffer_head_t *bh)
{
	struct ata_device_t *device;
	uint32_t start_sector;

	/* get ata device */
	device = ata_get_device(dev);
	if (!device || !device->read)
		return -EINVAL;

	/* get partition start sector */
	start_sector = ata_get_start_sector(device, dev);

	return device->read(device, bh, start_sector);
}

/*
 * Write to an ata device.
 */
int ata_write(dev_t dev, struct buffer_head_t *bh)
{
	struct ata_device_t *device;
	uint32_t start_sector;

	/* get ata device */
	device = ata_get_device(dev);
	if (!device || !device->write)
		return -EINVAL;

	/* get partition start sector */
	start_sector = ata_get_start_sector(device, dev);

	return device->write(device, bh, start_sector);
}

/*
 * Poll for identification.
 */
static int ata_poll_identify(struct ata_device_t *device)
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
static int ata_detect(struct ata_device_t *device)
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

	/* init drive */
	if (device->is_atapi)
		ret = ata_cd_init(device);
	else
		ret = ata_hd_init(device);

	/* free device on error */
	if (ret)
		goto err;

	/* set gendisk */
	device->hd.dev = mkdev(DEV_ATA_MAJOR, device->id << PARTITION_MINOR_SHIFT);
	sprintf(device->hd.name, "hd%c", 'a' + device->id);

	/* register device */
	if (!devfs_register(NULL, device->hd.name, S_IFBLK | 0660, device->hd.dev)) {
		ret = -ENOSPC;
		goto err;
	}

	return 0;
err:
	return ret;
}

/*
 * Irq handler.
 */
static void ata_irq_handler(struct registers_t *regs)
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

	/* detect hard drives */
	for (i = 0; i < NR_ATA_DEVICES; i++) {
		ret = ata_detect(&ata_devices[i]);
		if (ret)
			continue;

		/* set default block size */
		ata_blocksizes[i] = DEFAULT_BLOCK_SIZE;

		/* discover partitions */
		check_partition(&ata_devices[i].hd, ata_devices[i].hd.dev);
	}

	return 0;
}

/*
 * ATA file operations.
 */
static struct file_operations_t ata_fops = {
	.read		= generic_block_read,
	.write		= generic_block_write,
};

/*
 * ATA inode operations.
 */
struct inode_operations_t ata_iops = {
	.fops		= &ata_fops,
};
