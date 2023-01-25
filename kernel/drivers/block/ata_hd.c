#include <drivers/block/ata.h>
#include <drivers/pci/pci.h>
#include <x86/io.h>
#include <stderr.h>

/*
 * Read sectors from an ata device.
 */
static int ata_hd_read_sectors(struct ata_device_t *device, uint32_t sector, uint32_t nb_sectors, char *buf)
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
static int ata_hd_read(struct ata_device_t *device, struct buffer_head_t *bh)
{
	uint32_t nb_sectors, sector;

	/* compute nb sectors */
	nb_sectors = bh->b_size / ATA_SECTOR_SIZE;
	sector = bh->b_block * bh->b_size / ATA_SECTOR_SIZE;

	/* read sectors */
	return ata_hd_read_sectors(device, sector, nb_sectors, bh->b_data);
}

/*
 * Write sectors to an ata device.
 */
static int ata_hd_write_sectors(struct ata_device_t *device, uint32_t sector, uint32_t nb_sectors, char *buf)
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
static int ata_hd_write(struct ata_device_t *device, struct buffer_head_t *bh)
{
	uint32_t nb_sectors, sector;

	/* compute nb sectors */
	nb_sectors = bh->b_size / ATA_SECTOR_SIZE;
	sector = bh->b_block * bh->b_size / ATA_SECTOR_SIZE;

	/* write sectors */
	return ata_hd_write_sectors(device, sector, nb_sectors, bh->b_data);
}

/*
 * Init an ata hard disk.
 */
int ata_hd_init(struct ata_device_t *device)
{
	struct pci_device_t *ata_pci_device;
	uint32_t cmd_reg;
	int ret;
	
	/* get PCI device */
	ata_pci_device = pci_get_device(ATA_VENDOR_ID, ATA_DEVICE_ID);
	if (!ata_pci_device)
		return -EINVAL;

	/* select drive */
	outb(device->io_base + ATA_REG_HDDEVSEL, device->drive == ATA_MASTER ? 0xA0 : 0xB0);

	/* reset ata registers */
	outb(device->io_base + ATA_REG_SECCOUNT0, 0);
	outb(device->io_base + ATA_REG_LBA0, 0);
	outb(device->io_base + ATA_REG_LBA2, 0);
	outb(device->io_base + ATA_REG_LBA0, 0);

	/* identify drive */
	outb(device->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);

	/* poll for identification */
	ret = ata_poll_identify(device);
	if (ret)
		return ret;

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
	cmd_reg = pci_read_field(ata_pci_device->address, PCI_CMD);
	if (!(cmd_reg & (1 << 2))) {
		cmd_reg |= (1 << 2);
		pci_write_field(ata_pci_device->address, PCI_CMD, cmd_reg);
	}

	/* get BAR4 from pci device */
	device->bar4 = pci_read_field(ata_pci_device->address, PCI_BAR4);
	if (device->bar4 & 0x00000001)
		device->bar4 &= 0xFFFFFFFC;

	/* set operations */
	device->read = ata_hd_read;
	device->write = ata_hd_write;

	return 0;
}