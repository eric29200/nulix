#include <drivers/ata.h>
#include <drivers/pci.h>
#include <x86/interrupt.h>
#include <x86/io.h>
#include <mm/mm.h>
#include <stderr.h>
#include <string.h>
#include <stdio.h>
#include <dev.h>

/* ata devices */
static struct pci_device_t *pci_device = NULL;
static struct ata_device_t ata_devices[NR_ATA_DEVICES];
static size_t ata_blocksizes[NR_ATA_DEVICES] = { 0, };

/*
 * Get an ata device.
 */
static struct ata_device_t *ata_get_device(dev_t dev)
{
	if (major(dev) != DEV_ATA_MAJOR || minor(dev) >= NR_ATA_DEVICES)
		return NULL;

	return &ata_devices[minor(dev)];
}

/*
 * Read sectors from an ata device.
 */
static int ata_read_sectors(struct ata_device_t *device, uint32_t sector, uint32_t nb_sectors, char *buf)
{
	int status, dstatus;

	/* set transfert size */
	device->prdt[0].transfert_size = nb_sectors * ATA_SECTOR_SIZE;

	/* prepare DMA transfert */
	outb(device->bar4, 0);
	outl(device->bar4 + 0x04, (uint32_t) device->prdt);
	outb(device->bar4 + 0x02, inb(device->bar4 + 0x02) | 0x02 | 0x04);

	/* select sector */
	outb(device->io_base + ATA_REG_CONTROL, 0x00);
	outb(device->io_base + ATA_REG_HDDEVSEL, (device->drive == ATA_MASTER ? 0xE0 : 0xF0) | ((sector >> 24) & 0x0F));
	outb(device->io_base + ATA_REG_FEATURES, 0x00);
	outb(device->io_base + ATA_REG_SECCOUNT0, nb_sectors);
	outb(device->io_base + ATA_REG_LBA0, (uint8_t) sector);
	outb(device->io_base + ATA_REG_LBA1, (uint8_t) (sector >> 8));
	outb(device->io_base + ATA_REG_LBA2, (uint8_t) (sector >> 16));

	/* issue read DMA command */
	outb(device->io_base + ATA_REG_COMMAND, ATA_CMD_READ_DMA);
	outb(device->bar4, 0x8 | 0x1);

	/* wait for completion */
	for (;;) {
		status = inb(device->bar4 + 2);
		dstatus = inb(device->io_base + ATA_REG_STATUS);
		if (!(status & 0x04))
			continue;
		if (!(dstatus & ATA_SR_BSY))
			break;
	}

	/* copy buffer */
	memcpy(buf, device->buf, nb_sectors * ATA_SECTOR_SIZE);

	return 0;
}

/*
 * Read from an ata device.
 */
int ata_read(dev_t dev, struct buffer_head_t *bh)
{
	uint32_t nb_sectors, sector;
	struct ata_device_t *device;

	/* get ata device */
	device = ata_get_device(dev);
	if (!device)
		return -EINVAL;

	/* compute nb sectors */
	nb_sectors = bh->b_size / ATA_SECTOR_SIZE;
	sector = bh->b_block * bh->b_size / ATA_SECTOR_SIZE;

	/* read sectors */
	return ata_read_sectors(device, sector, nb_sectors, bh->b_data);
}

/*
 * Write sectors to an ata device.
 */
static int ata_write_sectors(struct ata_device_t *device, uint32_t sector, uint32_t nb_sectors, char *buf)
{
	int status, dstatus;

	/* copy buffer */
	memcpy(device->buf, buf, nb_sectors * ATA_SECTOR_SIZE);

	/* set transfert size */
	device->prdt[0].transfert_size = nb_sectors * ATA_SECTOR_SIZE;

	/* prepare DMA transfert */
	outb(device->bar4, 0);
	outl(device->bar4 + 0x04, (uint32_t) device->prdt);
	outb(device->bar4 + 0x02, inb(device->bar4 + 0x02) | 0x02 | 0x04);

	/* select sector */
	outb(device->io_base + ATA_REG_CONTROL, 0x00);
	outb(device->io_base + ATA_REG_HDDEVSEL, (device->drive == ATA_MASTER ? 0xE0 : 0xF0) | ((sector >> 24) & 0x0F));
	outb(device->io_base + ATA_REG_FEATURES, 0x00);
	outb(device->io_base + ATA_REG_SECCOUNT0, nb_sectors);
	outb(device->io_base + ATA_REG_LBA0, (uint8_t) sector);
	outb(device->io_base + ATA_REG_LBA1, (uint8_t) (sector >> 8));
	outb(device->io_base + ATA_REG_LBA2, (uint8_t) (sector >> 16));

	/* issue write DMA command */
	outb(device->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_DMA);
	outb(device->bar4, 0x1);

	/* wait for completion */
	for (;;) {
		status = inb(device->bar4 + 2);
		dstatus = inb(device->io_base + ATA_REG_STATUS);
		if (!(status & 0x04))
			continue;
		if (!(dstatus & ATA_SR_BSY))
			break;
	}

	return 0;
}

/*
 * Write to an ata device.
 */
int ata_write(dev_t dev, struct buffer_head_t *bh)
{
	uint32_t nb_sectors, sector;
	struct ata_device_t *device;

	/* get ata device */
	device = ata_get_device(dev);
	if (!device)
		return -EINVAL;

	/* compute nb sectors */
	nb_sectors = bh->b_size / ATA_SECTOR_SIZE;
	sector = bh->b_block * bh->b_size / ATA_SECTOR_SIZE;

	/* write sectors */
	return ata_write_sectors(device, sector, nb_sectors, bh->b_data);
}

/*
 * Identify a device.
 */
static int ata_identify(struct ata_device_t *device)
{
	uint32_t cmd_reg;
	uint8_t status;
	int i;

	/* select drive */
	if (device->drive == ATA_MASTER)
		outb(device->io_base + ATA_REG_HDDEVSEL, 0xA0);
	else
		outb(device->io_base + ATA_REG_HDDEVSEL, 0xB0);

	/* reset ata registers */
	outb(device->io_base + ATA_REG_SECCOUNT0, 0);
	outb(device->io_base + ATA_REG_LBA0, 0);
	outb(device->io_base + ATA_REG_LBA2, 0);
	outb(device->io_base + ATA_REG_LBA0, 0);

	/* identify drive */
	outb(device->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

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

	/* allocate prdt */
	device->prdt = kmalloc_align(sizeof(struct ata_prdt_t));
	if (!device->prdt)
		return -ENOMEM;

	/* allocate buffer */
	device->buf = kmalloc_align(PAGE_SIZE);
	if (!device->buf) {
		kfree(device->prdt);
		return -ENOMEM;
	}

	/* clear prdt and buffer */
	memset(device->prdt, 0, sizeof(struct ata_prdt_t));
	memset(device->buf, 0, PAGE_SIZE);

	/* set prdt */
	device->prdt[0].buffer_phys = (uint32_t) device->buf;
	device->prdt[0].transfert_size = ATA_SECTOR_SIZE;
	device->prdt[0].mark_end = 0x8000;

	/* activate pci */
	cmd_reg = pci_read_field(pci_device->address, PCI_CMD);
	if (!(cmd_reg & (1 << 2))) {
		cmd_reg |= (1 << 2);
		pci_write_field(pci_device->address, PCI_CMD, cmd_reg);
	}

	/* get BAR4 from pci device */
	device->bar4 = pci_read_field(pci_device->address, PCI_BAR4);
	if (device->bar4 & 0x00000001)
		device->bar4 &= 0xFFFFFFFC;

	return 0;
}

/*
 * Detect an ATA device.
 */
static int ata_detect(int id, uint16_t bus, uint8_t drive)
{
	int ret;

	if (id > NR_ATA_DEVICES)
		return -ENXIO;

	/* set device */
	ata_devices[id].bus = bus;
	ata_devices[id].drive = drive;
	ata_devices[id].io_base = bus == ATA_PRIMARY ? ATA_PRIMARY_IO : ATA_SECONDARY_IO;

	/* identify device */
	ret = ata_identify(&ata_devices[id]);
	if (ret)
		memset(&ata_devices[id], 0, sizeof(struct ata_device_t));

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

	/* get PCI device */
	pci_device = pci_get_device(ATA_VENDOR_ID, ATA_DEVICE_ID);
	if (!pci_device)
		return -EINVAL;

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
};

/*
 * ATA inode operations.
 */
struct inode_operations_t ata_iops = {
	.fops		= &ata_fops,
};