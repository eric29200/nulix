#include <drivers/pci/pci.h>
#include <mm/mm.h>
#include <x86/io.h>
#include <string.h>
#include <stderr.h>
#include <stdio.h>

/* PCI devices array */
static LIST_HEAD(pci_devices);

/*
 * Create a PCI address.
 */
static inline uint32_t pci_get_address(uint8_t bus, uint8_t device, uint8_t func)
{
	return (bus << 16) | (device << 11) | (func << 8) | 0x80000000;
}

/*
 * Read a PCI byte.
 */
uint8_t pci_read_byte(uint32_t address, uint8_t offset)
{
	outl(PCI_ADDRESS_PORT, address | (offset & 0xFC));
	return inb(PCI_VALUE_PORT);
}

/*
 * Read a PCI word.
 */
uint16_t pci_read_word(uint32_t address, uint8_t offset)
{
	outl(PCI_ADDRESS_PORT, address | (offset & 0xFC));
	return inl(PCI_VALUE_PORT);
}

/*
 * Read a PCI double word.
 */
uint32_t pci_read_dword(uint32_t address, uint8_t offset)
{
	outl(PCI_ADDRESS_PORT, address | (offset & 0xFC));
	return inl(PCI_VALUE_PORT);
}

/*
 * Write a PCI byte.
 */
void pci_write_byte(uint32_t address, uint8_t offset, uint8_t value)
{
	outl(PCI_ADDRESS_PORT, address | (offset & 0xFC));
	outb(PCI_VALUE_PORT, value);
}

/*
 * Write a PCI word.
 */
void pci_write_word(uint32_t address, uint8_t offset, uint16_t value)
{
	outl(PCI_ADDRESS_PORT, address | (offset & 0xFC));
	outw(PCI_VALUE_PORT, value);
}

/*
 * Write a PCI double word.
 */
void pci_write_dword(uint32_t address, uint8_t offset, uint32_t value)
{
	outl(PCI_ADDRESS_PORT, address | (offset & 0xFC));
	outl(PCI_VALUE_PORT, value);
}

/*
 * Enables bus mastering.
 */
void pci_set_master(struct pci_device *pci_dev)
{
	uint16_t cmd;

	/* already activated */
	cmd = pci_read_word(pci_dev->address, PCI_COMMMAND);
	if (cmd & PCI_CMD_MASTER)
		return;

	/* activate */
	cmd |= PCI_CMD_MASTER;
	pci_write_word(pci_dev->address, PCI_COMMMAND, cmd);
}

/*
 * Enables device.
 */
void pci_enable_device(struct pci_device *pci_dev)
{
	uint16_t cmd;

	/* already activated */
	cmd = pci_read_word(pci_dev->address, PCI_COMMMAND);
	if (cmd & PCI_CMD_IO & PCI_CMD_MEMORY)
		return;

	/* activate */
	cmd |= PCI_CMD_IO | PCI_CMD_MEMORY;
	pci_write_word(pci_dev->address, PCI_COMMMAND, cmd);
}

/*
 * Scan a PCI bus and register devices.
 */
static int pci_scan_bus(uint8_t bus)
{
	struct pci_device *pci_dev;
	uint32_t address, reg, l;
	uint8_t device, func;

	/* parse all devices/functions slots */
	for (device = 0; device < 32; device++) {
		for (func = 0; func < 8; func++) {
			address = pci_get_address(bus, device, func);

			/* get vendor/device id */
			l = pci_read_dword(address, PCI_VENDOR_ID);
			if ((l & 0xFFFF) == PCI_INVALID_VENDOR)
				continue;

			/* allocate a new pci device */
			pci_dev = (struct pci_device *) kmalloc(sizeof(struct pci_device));
			if (!pci_dev)
				return -ENOMEM;

			/* set pci device */
			memset(pci_dev, 0, sizeof(struct pci_device));
			pci_dev->address = address;
			pci_dev->vendor_id = l & 0xFFFF;
			pci_dev->device_id = (l >> 16) & 0xFFFF;
			pci_dev->irq = pci_read_byte(address, PCI_INTERRUPT_LINE);
			pci_dev->class = pci_read_dword(address, PCI_CLASS_REVISION) >> 8;
			for (reg = 0; reg < 6; reg++)
				pci_dev->bar[reg] = pci_read_dword(address, PCI_BAR0 + (reg << 2));

			/* print device */
			printk("PCI device (vendor = 0x%x, device = 0x%x, BAR = 0x%x)\n", pci_dev->vendor_id, pci_dev->device_id, pci_dev->bar[0]);

			/* add device */
			list_add_tail(&pci_dev->list, &pci_devices);
		}
	}

	return 0;
}

/*
 * Check if a PCI device matches a PCI table id.
 */
static struct pci_device_id *pci_match_device(struct pci_device_id *ids, struct pci_device *dev)
{
	while (ids->vendor) {
		if (ids->vendor == dev->vendor_id && ids->device == dev->device_id)
			return ids;
		ids++;
	}

	return NULL;
}

/*
 * Announce a PCI device.
 */
static int pci_announce_device(struct pci_driver *drv, struct pci_device *dev)
{
	struct pci_device_id *id = NULL;
	int ret = 0;

	/* check if device matches driver */
	if (drv->id_table) {
		id = pci_match_device(drv->id_table, dev);
		if (!id)
			return 0;
	}

	/* probe device */
	if (drv->probe(dev, id) >= 0) {
		dev->driver = drv;
		ret = 1;
	}

	return ret;
}

/*
 * Register a PCI driver (returns number of devices attached).
 */
int pci_register_driver(struct pci_driver *drv)
{
	struct pci_device *pci_dev;
	struct list_head *pos;
	int ret = 0;

	if (!drv->id_table)
		return -EINVAL;

	list_for_each(pos, &pci_devices) {
		pci_dev = list_entry(pos, struct pci_device, list);
		ret += pci_announce_device(drv, pci_dev);
	}

	return ret;
}

/*
 * Find a PCI device.
 */
struct pci_device *pci_find_class(uint32_t class, struct pci_device *from)
{
	struct list_head *start, *pos;
	struct pci_device *dev;

	if (from)
		start = from->list.next;
	else
		start = &pci_devices;

	list_for_each(pos, start) {
		dev = list_entry(pos, struct pci_device, list);
		if (dev->class == class)
			return dev;
	}

	return NULL;
}

/*
 * Init PCI devices.
 */
int init_pci()
{
	/* scan first bus */
	return pci_scan_bus(0);
}
