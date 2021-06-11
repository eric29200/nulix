#include <drivers/rtl8139.h>
#include <drivers/pci.h>
#include <x86/interrupt.h>
#include <x86/io.h>
#include <net/net.h>
#include <net/sk_buff.h>
#include <mm/mm.h>
#include <stderr.h>
#include <string.h>

/* Realtek 8139 device */
static struct net_device_t *rtl8139_net_dev = NULL;

/* transmit buffers */
static void *tx_buffer[4];
static uint32_t tx_buffers_phys[4];
static int tx_cur = 0;

/* receive buffer */
static void *rx_buffer = NULL;

/*
 * Send a packet.
 */
static void rtl8139_send_packet(struct sk_buff_t *skb)
{
  /* copy packet to tx buffer */
  memcpy(tx_buffer[tx_cur], skb->head, skb->len);

  /* put packet on device */
  outl(rtl8139_net_dev->io_base + 0x20 + tx_cur * 4, tx_buffers_phys[tx_cur]);
  outl(rtl8139_net_dev->io_base + 0x10 + tx_cur * 4, skb->size);

  /* update tx buffer index */
  tx_cur = tx_cur >= 3 ? 0 : tx_cur + 1;
}

/*
 * Receive a packet.
 */
static void rtl8139_receive_packet()
{
  struct rtl8139_rx_header_t *rx_header;
  struct sk_buff_t *skb;
  uint16_t rx_buf_ptr;

  /* handle all received packets */
  while ((inb(rtl8139_net_dev->io_base + 0x37) & 0x01) == 0) {
    /* get packet header */
    rx_buf_ptr = inw(rtl8139_net_dev->io_base + 0x38) + 0x10;
    rx_header = (struct rtl8139_rx_header_t *) (rx_buffer + rx_buf_ptr);
    rx_buf_ptr = (rx_buf_ptr + rx_header->size + sizeof(struct rtl8139_rx_header_t) + 3) & ~(0x3);

    /* allocate a socket buffer */
    skb = skb_alloc(rx_header->size);
    if (skb) {
      /* set network device */
      skb->dev = rtl8139_net_dev;

      /* copy data into socket buffer */
      skb_put(skb, rx_header->size);
      memcpy(skb->data, ((void *) rx_header) + sizeof(struct rtl8139_rx_header_t), rx_header->size);

      /* handle socket buffer */
      skb_handle(skb);

      /* free socket buffer */
      skb_free(skb);
    }

    /* update received buffer pointer */
    outw(rtl8139_net_dev->io_base + 0x38, rx_buf_ptr - 0x10);
  }
}

/*
 * Realtek 8139 IRQ handler.
 */
void rtl8139_irq_handler(struct registers_t *regs)
{
  int status;

  UNUSED(regs);

  /* get status */
  status = inw(rtl8139_net_dev->io_base + 0x3E);
  if (!status)
    return;

  /* ack/reset interrupt */
  outw(rtl8139_net_dev->io_base + 0x3E, status);

  /* handle reception */
  if (status & ROK)
    rtl8139_receive_packet();
}

/*
 * Get Realtek 8139 network device.
 */
struct net_device_t *rtl8139_get_net_device()
{
  return rtl8139_net_dev;
}

/*
 * Init Realtek 8139 device.
 */
int init_rtl8139(uint8_t *ip_addr)
{
  uint32_t io_base, pci_cmd, rx_buffer_phys;
  struct pci_device_t *pci_dev;
  int i;

  /* get pci device */
  pci_dev = pci_get_device(RTL8139_VENDOR_ID, RTL8139_DEVICE_ID);
  if (!pci_dev)
    return -EINVAL;

  /* get I/O base address */
  io_base = pci_dev->bar0 & ~(0x3);

  /* register net device */
  rtl8139_net_dev = register_net_device(io_base);
  if (!rtl8139_net_dev)
    return -ENOSPC;

  /* get mac address */
  for (i = 0; i < 6; i++)
    rtl8139_net_dev->mac_addr[i] = inb(io_base + RTL8139_MAC_ADDRESS + i);

  /* set ip address */
  memcpy(rtl8139_net_dev->ip_addr, ip_addr, 4);

  /* set methods */
  rtl8139_net_dev->send_packet = rtl8139_send_packet;

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

  /* allocate transmit buffers */
  for (i = 0; i < 4; i++) {
    tx_buffer[i] = kmalloc_align_phys(PAGE_SIZE, &tx_buffers_phys[i]);
    if (!tx_buffer[i]) {
      while (--i >= 0)
        kfree(tx_buffer[i]);
      kfree(rx_buffer);
      return -ENOMEM;
    }
  }

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
  rtl8139_net_dev->irq = pci_read_field(pci_dev->address, PCI_INTERRUPT_LINE);

  /* register interrupt handler */
  register_interrupt_handler(IRQ0 + rtl8139_net_dev->irq, rtl8139_irq_handler);

  return 0;
}
