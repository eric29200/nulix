#ifndef _PCI_H_
#define _PCI_H_

#include <stddef.h>

#define NR_PCI_DEVICES        32

#define PCI_ADDRESS_PORT      0xCF8
#define PCI_VALUE_PORT        0xCFC
#define PCI_INVALID_VENDOR    0xFFFF

/*
 * PCI device structure.
 */
struct pci_device_t {
  uint32_t address;
  uint32_t device_id;
  uint32_t vendor_id;
};

void init_pci();

#endif
