#include <drivers/pci.h>
#include <x86/io.h>
#include <string.h>
#include <stdio.h>

/* PCI devices array */
static struct pci_device_t pci_devices[NR_PCI_DEVICES];
static int nb_pci_devices = 0;

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
static inline uint32_t pci_read_field(uint32_t address, uint8_t  offset)
{
  outl(PCI_ADDRESS_PORT, address | (offset & 0xFC));
  return inl(PCI_VALUE_PORT);
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
 * Scan a PCI bus and register devices.
 */
static void pci_scan_bus(uint8_t bus)
{
  uint16_t vendor_id, device_id;
  uint8_t device, func;
  uint32_t address;

  /* parse all devices/functions slots */
  for (device = 0; device < 32; device++) {
    for (func = 0; func < 8; func++) {
      address = pci_get_address(bus, device, func);

      /* get vendor id */
      vendor_id = pci_get_vendor_id(address);
      if (vendor_id == PCI_INVALID_VENDOR)
        continue;

      /* get device id */
      device_id = pci_get_device_id(address);

      if (nb_pci_devices >= NR_PCI_DEVICES) {
        printf("PCI device (vendor id = %x, device id = %x) cannot be registered : too many devices\n",
               vendor_id, device_id);
        continue;
      }

      /* register PCI device */
      pci_devices[nb_pci_devices].address = address;
      pci_devices[nb_pci_devices].vendor_id = vendor_id;
      pci_devices[nb_pci_devices].device_id = device_id;
      nb_pci_devices++;

      printf("PCI device (vendor id = %x, device id = %x) registered\n", vendor_id, device_id);
    }
  }
}

/*
 * Init PCI devices.
 */
void init_pci()
{
  /* reset pci devices */
  memset(pci_devices, 0, sizeof(struct pci_device_t) * NR_PCI_DEVICES);

  /* scan first bus */
  pci_scan_bus(0);
}
