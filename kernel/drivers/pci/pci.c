#include <drivers/pci/pci.h>
#include <x86/io.h>
#include <string.h>
#include <stderr.h>
#include <stdio.h>

/* PCI devices array */
static struct pci_device pci_devices[NR_PCI_DEVICES];
static int nr_pci_devices = 0;

/*
 * Create a PCI address.
 */
static inline uint32_t pci_get_address(uint8_t bus, uint8_t device, uint8_t func)
{
	return (bus << 16) | (device << 11) | (func << 8) | 0x80000000;
}

/*
 * Read a PCI field.
 */
uint32_t pci_read_field(uint32_t address, uint8_t offset)
{
	outl(PCI_ADDRESS_PORT, address | (offset & 0xFC));
	return inl(PCI_VALUE_PORT);
}

/*
 * Write a PCI field.
 */
void pci_write_field(uint32_t address, uint8_t offset, uint32_t value)
{
	outl(PCI_ADDRESS_PORT, address | (offset & 0xFC));
	outl(PCI_VALUE_PORT, value);
}

/*
 * Get PCI vendor id.
 */
static inline uint16_t pci_get_vendor_id(uint32_t address)
{
	return pci_read_field(address, 0) & 0xFFFF;
}

/*
 * Get PCI device id.
 */
static inline uint16_t pci_get_device_id(uint32_t address)
{
	return (pci_read_field(address, 0) >> 16) & 0xFFFF;
}

/*
 * Enables bus mastering.
 */
void pci_set_master(struct pci_device *pci_dev)
{
	uint16_t cmd;

	/* already activated */
	cmd = pci_read_field(pci_dev->address, PCI_CMD);
	if (cmd & PCI_CMD_MASTER)
		return;

	/* activate */
	cmd |= PCI_CMD_MASTER;
	pci_write_field(pci_dev->address, PCI_CMD, cmd);
}

/*
 * Enables device.
 */
void pci_enable_device(struct pci_device *pci_dev)
{
	uint16_t cmd;

	/* already activated */
	cmd = pci_read_field(pci_dev->address, PCI_CMD);
	if (cmd & PCI_CMD_IO & PCI_CMD_MEMORY)
		return;

	/* activate */
	cmd |= PCI_CMD_IO | PCI_CMD_MEMORY;
	pci_write_field(pci_dev->address, PCI_CMD, cmd);
}

/*
 * Scan a PCI bus and register devices.
 */
static void pci_scan_bus(uint8_t bus)
{
	uint16_t vendor_id, device_id;
	uint32_t address, bar0;
	uint8_t device, func;

	/* parse all devices/functions slots */
	for (device = 0; device < 32; device++) {
		for (func = 0; func < 8; func++) {
			address = pci_get_address(bus, device, func);

			/* get vendor id */
			vendor_id = pci_get_vendor_id(address);
			if (vendor_id == PCI_INVALID_VENDOR)
				continue;

			/* get device id and first Base Address Register */
			device_id = pci_get_device_id(address);
			bar0 = pci_read_field(address, PCI_BAR0);

			if (nr_pci_devices >= NR_PCI_DEVICES) {
				printf("PCI device (vendor id = 0x%x, device id = 0x%x) cannot be registered : too many devices\n", vendor_id, device_id);
				continue;
			}

			/* register PCI device */
			pci_devices[nr_pci_devices].address = address;
			pci_devices[nr_pci_devices].vendor_id = vendor_id;
			pci_devices[nr_pci_devices].device_id = device_id;
			pci_devices[nr_pci_devices].bar0 = bar0;
			nr_pci_devices++;

			printf("PCI device (vendor id = 0x%x, device id = 0x%x, BAR = 0x%x) registered\n", vendor_id, device_id, bar0);
		}
	}
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
	int i, ret = 0;

	if (!drv->id_table)
		return -EINVAL;

	for (i = 0; i < nr_pci_devices; i++)
		ret += pci_announce_device(drv, &pci_devices[i]);

	return ret;
}

/*
 * Init PCI devices.
 */
void init_pci()
{
	/* reset pci devices */
	memset(pci_devices, 0, sizeof(struct pci_device) * NR_PCI_DEVICES);

	/* scan first bus */
	pci_scan_bus(0);
}
