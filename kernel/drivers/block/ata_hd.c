#include <drivers/block/ata.h>
#include <drivers/pci/pci.h>
#include <x86/io.h>
#include <stderr.h>

#define ORDER_DMA_PAGES		2
#define NR_DMA_PAGES		(1 << (ORDER_DMA_PAGES))
#define NR_DMA_SECTORS		((NR_DMA_PAGES) * PAGE_SIZE / ATA_SECTOR_SIZE)

/*
 * Wait for operation completion.
 */
static void ata_hd_wait(struct ata_device *device)
{
	int status, dstatus;

	for (;;) {
		status = inb(device->bar4 + 2);
		dstatus = inb(device->io_base + ATA_REG_STATUS);

		if (!(status & 0x04))
			continue;

		if (!(dstatus & ATA_SR_BSY))
			break;
	}
}

/*
 * Read from an ata device.
 */
static int ata_hd_read(struct ata_device *device, uint32_t sector, size_t nr_sectors, char *buf)
{
	uint32_t nsect;

	while (nr_sectors) {
		/* limit transfert to buffer size */
		nsect = nr_sectors;
		if (nsect > NR_DMA_SECTORS)
			nsect = NR_DMA_SECTORS;

		/* set transfert size */
		device->prdt[0].transfert_size = nsect * ATA_SECTOR_SIZE;

		/* prepare DMA transfert */
		outb(device->bar4, 0);
		outl(device->bar4 + 0x04, __pa(device->prdt));
		outb(device->bar4 + 0x02, inb(device->bar4 + 0x02) | 0x02 | 0x04);

		/* select sector */
		outb(device->io_base + ATA_REG_CONTROL, 0x00);
		outb(device->io_base + ATA_REG_HDDEVSEL, (device->drive == ATA_MASTER ? 0xE0 : 0xF0) | ((sector >> 24) & 0x0F));
		outb(device->io_base + ATA_REG_FEATURES, 0x00);
		outb(device->io_base + ATA_REG_SECCOUNT0, nsect);
		outb(device->io_base + ATA_REG_LBA0, (uint8_t) sector);
		outb(device->io_base + ATA_REG_LBA1, (uint8_t) (sector >> 8));
		outb(device->io_base + ATA_REG_LBA2, (uint8_t) (sector >> 16));

		/* issue read DMA command */
		outb(device->io_base + ATA_REG_COMMAND, ATA_CMD_READ_DMA);
		outb(device->bar4, 0x8 | 0x1);

		/* wait for completion */
		ata_hd_wait(device);

		/* copy buffer */
		memcpy(buf, device->buf, nsect * ATA_SECTOR_SIZE);

		/* update size */
		buf += nsect * ATA_SECTOR_SIZE;
		sector += nsect;
		nr_sectors -= nsect;
	}

	return 0;
}

/*
 * Write to an ata device.
 */
static int ata_hd_write(struct ata_device *device, uint32_t sector, size_t nr_sectors, char *buf)
{
	uint32_t nsect;

	while (nr_sectors) {
		/* limit transfert to buffer size */
		nsect = nr_sectors;
		if (nsect > NR_DMA_SECTORS)
			nsect = NR_DMA_SECTORS;

		/* copy buffer */
		memcpy(device->buf, buf, nsect * ATA_SECTOR_SIZE);

		/* set transfert size */
		device->prdt[0].transfert_size = nsect * ATA_SECTOR_SIZE;

		/* prepare DMA transfert */
		outb(device->bar4, 0);
		outl(device->bar4 + 0x04, __pa(device->prdt));
		outb(device->bar4 + 0x02, inb(device->bar4 + 0x02) | 0x02 | 0x04);

		/* select sector */
		outb(device->io_base + ATA_REG_CONTROL, 0x00);
		outb(device->io_base + ATA_REG_HDDEVSEL, (device->drive == ATA_MASTER ? 0xE0 : 0xF0) | ((sector >> 24) & 0x0F));
		outb(device->io_base + ATA_REG_FEATURES, 0x00);
		outb(device->io_base + ATA_REG_SECCOUNT0, nsect);
		outb(device->io_base + ATA_REG_LBA0, (uint8_t) sector);
		outb(device->io_base + ATA_REG_LBA1, (uint8_t) (sector >> 8));
		outb(device->io_base + ATA_REG_LBA2, (uint8_t) (sector >> 16));

		/* issue write DMA command */
		outb(device->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_DMA);
		outb(device->bar4, 0x1);

		/* wait for completion */
		ata_hd_wait(device);

		/* update size */
		buf += nsect * ATA_SECTOR_SIZE;
		sector += nsect;
		nr_sectors -= nsect;
	}

	return 0;
}

/*
 * Init an ata hard disk.
 */
int ata_hd_init(struct ata_device *device)
{
	struct pci_device *ata_pci_device;
	uint32_t cmd_reg;

	/* no sectors */
	if (!device->identify.sectors_28 && !device->identify.sectors_48)
		return -EINVAL;

	/* get PCI device */
	ata_pci_device = pci_get_device(ATA_VENDOR_ID, ATA_DEVICE_ID);
	if (!ata_pci_device)
		return -EINVAL;

	/* allocate prdt */
	device->prdt = kmalloc(sizeof(struct ata_prdt));
	if (!device->prdt)
		return -ENOMEM;

	/* allocate buffer */
	device->buf = get_free_pages(ORDER_DMA_PAGES);
	if (!device->buf) {
		kfree(device->prdt);
		return -ENOMEM;
	}

	/* clear prdt and buffer */
	memset(device->prdt, 0, sizeof(struct ata_prdt));
	memset(device->buf, 0, NR_DMA_PAGES * PAGE_SIZE);

	/* set prdt */
	device->prdt[0].buffer_phys = __pa(device->buf);
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
	device->sector_size = ATA_SECTOR_SIZE;
	device->read = ata_hd_read;
	device->write = ata_hd_write;

	return 0;
}
