#include <drivers/rtl8139.h>
#include <drivers/pci.h>
#include <x86/interrupt.h>
#include <x86/io.h>
#include <mm/mm.h>
#include <stderr.h>
#include <string.h>

/*
 * Realtek 8139 IRQ handler.
 */
void rtl_8139_irq_handler(struct registers_t *regs)
{
  UNUSED(regs);
}

/*
 * Init Realtek 8139 device.
 */
int init_rtl8139()
{
  uint32_t io_base, pci_cmd, rx_buffer_phys;
  struct pci_device_t *pci_dev;
  void *rx_buffer;
  uint8_t irq;

  /* get pci device */
  pci_dev = pci_get_device(RTL8139_VENDOR_ID, RTL8139_DEVICE_ID);
  if (!pci_dev)
    return -EINVAL;

  /* get I/O base address */
  io_base = pci_dev->bar0 & ~(0x3);

  /* enable PCI Bus Mastering to allow NIC to perform DMA */
  pci_cmd = pci_read_field(pci_dev->address, PCI_CMD);
  if (!(pci_cmd & PCI_CMD_REG_BUS_MASTER)) {
    pci_cmd |= PCI_CMD_REG_BUS_MASTER;
    pci_write_field(pci_dev->address, PCI_CMD, pci_cmd);
  }

  /* power on the device */
  outb(io_base + 0x52, 0x0);

  /* software reset (to clean up remaining buffers) */
  outb(io_base + 0x37, 0x10);
  while (inb(io_base + 0x37) & 0x10);

  /* allocate receive buffer */
  rx_buffer = kmalloc_align_phys(RX_BUFFER_SIZE, &rx_buffer_phys);
  if (!rx_buffer)
    return -ENOMEM;

  /* memzero buffer and set physical address on chip */
  memset(rx_buffer, 0, RX_BUFFER_SIZE);
  outl(io_base + 0x30, rx_buffer_phys);

  /* set Interrupt Mask Register (only accept Transmit OK and Receive OK interrupts) */
  outw(io_base + 0x3C, 0x0005);

  /* configure receive buffer = accept Broadcast/Multicast/Pysical/All packets */
  outl(io_base + 0x44, 0xF | (1 << 7));

  /* enable receive and transmitter (to accept and transmit packets) */
  outb(io_base + 0x37, 0x0C);

  /* get PCI interrupt line */
  irq = pci_read_field(pci_dev->address, PCI_INTERRUPT_LINE);

  /* register interrupt handler */
  register_interrupt_handler(irq, rtl_8139_irq_handler);

  return 0;
}
