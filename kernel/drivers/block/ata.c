#include <drivers/block/ata.h>
#include <x86/interrupt.h>
#include <x86/io.h>
#include <mm/mm.h>
#include <stderr.h>
#include <string.h>
#include <stdio.h>
#include <dev.h>

/* ata devices */
static struct ata_device_t *ata_devices[NR_ATA_DEVICES];
static size_t ata_blocksizes[NR_ATA_DEVICES] = { 0, };

/*
 * Get an ata device.
 */
struct ata_device_t *ata_get_device(dev_t dev)
{
	if (major(dev) != DEV_ATA_MAJOR || minor(dev) >= NR_ATA_DEVICES)
		return NULL;

	return ata_devices[minor(dev)];
}

/*
 * Read from an ata device.
 */
int ata_read(dev_t dev, struct buffer_head_t *bh)
{
	struct ata_device_t *device;

	/* get ata device */
	device = ata_get_device(dev);
	if (!device || !device->read)
		return -EINVAL;

	return device->read(device, bh);
}

/*
 * Write to an ata device.
 */
int ata_write(dev_t dev, struct buffer_head_t *bh)
{
	struct ata_device_t *device;

	/* get ata device */
	device = ata_get_device(dev);
	if (!device || !device->write)
		return -EINVAL;

	return device->write(device, bh);
}

/*
 * Poll for identification.
 */
int ata_poll_identify(struct ata_device_t *device)
{
	uint8_t status;
	int i;

	/* wait until BSY is clear */
	while (1) {
		status = inb(device->io_base + ATA_REG_STATUS);
		if (!status)
			return -ENXIO;

		if (!(status & ATA_SR_BSY))
			break;
	}

	/* wait until DRQ (drive has data to transfer) is clear */
	while (1) {
		status = inb(device->io_base + ATA_REG_STATUS);
		if (status & ATA_SR_ERR)
			return -EFAULT;

		if (status & ATA_SR_DRQ)
			break;
	}

	/* read identified drive data */
	for (i = 0; i < 256; i += 2)
		inw(device->io_base + ATA_REG_DATA);

	return 0;
}

/*
 * Detect an ATA device.
 */
static int ata_detect(int id, uint16_t bus, uint8_t drive)
{
	struct ata_device_t *device;
	int ret;

	/* check id */
	if (id > NR_ATA_DEVICES)
		return -ENXIO;

	/* allocate a new device */
	device = (struct ata_device_t *) kmalloc(sizeof(struct ata_device_t));
	if (!device)
		return -ENOMEM;

	/* set device */
	device->bus = bus;
	device->drive = drive;
	device->io_base = bus == ATA_PRIMARY ? ATA_PRIMARY_IO : ATA_SECONDARY_IO;

	/* try to identify hard disk */
	ret = ata_hd_init(device);
	if (!ret)
		goto out;

	/* try to identify cdrom */
	ret = ata_cd_init(device);
	if (!ret)
		goto out;

	/* free device */
	kfree(device);
	device = NULL;
out:
	ata_devices[id] = device;
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
	int i;

	/* reset ata devices */
	memset(ata_devices, 0, sizeof(ata_devices));

	/* register interrupt handlers */
	register_interrupt_handler(IRQ14, ata_irq_handler);
	register_interrupt_handler(IRQ15, ata_irq_handler);

	/* detect hard drives */
	if (ata_detect(0, ATA_PRIMARY, ATA_MASTER) == 0)
		printf("[Kernel] Primary ATA master drive detected\n");
	if (ata_detect(1, ATA_PRIMARY, ATA_SLAVE) == 0)
		printf("[Kernel] Primary ATA slave drive detected\n");
	if (ata_detect(2, ATA_SECONDARY, ATA_MASTER) == 0)
		printf("[Kernel] Secondary ATA master drive detected\n");
	if (ata_detect(3, ATA_SECONDARY, ATA_SLAVE) == 0)
		printf("[Kernel] Secondary ATA slave drive detected\n");

	/* set default block size */
	for (i = 0; i < NR_ATA_DEVICES; i++)
		ata_blocksizes[i] = DEFAULT_BLOCK_SIZE;
	blocksize_size[DEV_ATA_MAJOR] = ata_blocksizes;

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
