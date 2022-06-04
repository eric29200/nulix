#ifndef _PCI_H_
#define _PCI_H_

#include <stddef.h>

#define NR_PCI_DEVICES			32

#define PCI_ADDRESS_PORT		0xCF8
#define PCI_VALUE_PORT			0xCFC
#define PCI_CMD				0x04
#define PCI_STATUS			0x06
#define PCI_BAR0			0x10

#define PCI_CMD_REG_BUS_MASTER		(1 << 2)
#define PCI_INTERRUPT_LINE		0x3C

#define PCI_INVALID_VENDOR		0xFFFF

/*
 * PCI device structure.
 */
struct pci_device_t {
	uint32_t address;
	uint32_t device_id;
	uint32_t vendor_id;
	uint32_t bar0;
};

void init_pci();
struct pci_device_t *pci_get_device(uint32_t vendor_id, uint32_t device_id);
uint32_t pci_read_field(uint32_t address, uint8_t offset);
void pci_write_field(uint32_t address, uint8_t offset, uint32_t value);

#endif
