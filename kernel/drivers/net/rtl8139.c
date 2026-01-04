#include <drivers/net/rtl8139.h>
#include <drivers/pci/pci.h>
#include <proc/sched.h>
#include <x86/interrupt.h>
#include <x86/io.h>
#include <net/inet/net.h>
#include <net/inet/arp.h>
#include <net/sk_buff.h>
#include <mm/paging.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>

/* Realtek 8139 device */
static struct net_device *rtl8139_net_dev = NULL;

/* Realtek 8139 registers */
enum RTL8129_registers {
	MAC0 = 0,
	MAR0 = 8,
	TxStatus0 = 0x10,
	TxAddr0 = 0x20,
	RxBuf = 0x30, RxEarlyCnt = 0x34, RxEarlyStatus = 0x36,
	ChipCmd = 0x37, RxBufPtr = 0x38, RxBufAddr = 0x3A,
	IntrMask = 0x3C, IntrStatus = 0x3E,
	TxConfig = 0x40, RxConfig = 0x44,
	Timer = 0x48,
	RxMissed = 0x4C,
	Cfg9346 = 0x50, Config0 = 0x51, Config1 = 0x52,
	FlashReg = 0x54, GPPinData = 0x58, GPPinDir = 0x59, MII_SMI = 0x5A, HltClk = 0x5B,
	MultiIntr = 0x5C, TxSummary = 0x60,
	MII_BMCR = 0x62, MII_BMSR = 0x64, NWayAdvert = 0x66, NWayLPAR = 0x68,
	NWayExpansion = 0x6A,
	FIFOTMS = 0x70,
	CSCR = 0x74,
	PARA78 = 0x78, PARA7c = 0x7c,
};

/* Realtek 8139 commands */
enum ChipCmdBits {
	CmdReset = 0x10, CmdRxEnb = 0x08, CmdTxEnb = 0x04, RxBufEmpty = 0x01,
};

/* Realtek 8139 interrupt register bits */
enum IntrStatusBits {
	PCIErr = 0x8000, PCSTimeout = 0x4000,
	RxFIFOOver = 0x40, RxUnderrun = 0x20, RxOverflow = 0x10,
	TxErr = 0x08, TxOK = 0x04, RxErr = 0x02, RxOK = 0x01,
};

/*
 * Send a socket buffer.
 */
static int rtl8139_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	struct rtl8139_private *tp = (struct rtl8139_private *) dev->private;

	/* copy packet to tx buffer */
	memcpy(tp->tx_buf[tp->cur_tx], skb->head, skb->len);

	/* put packet on device */
	outl(rtl8139_net_dev->io_base + TxAddr0 + tp->cur_tx * 4, __pa(tp->tx_buf[tp->cur_tx]));
	outl(rtl8139_net_dev->io_base + TxStatus0 + tp->cur_tx * 4, skb->size);

	/* update stats */
	dev->stats.tx_packets++;
	dev->stats.tx_bytes += skb->size;

	/* update tx buffer index */
	tp->cur_tx++;
	if (tp->cur_tx >= NUM_TX_DESC)
		tp->cur_tx = 0;

	return 0;
}

/*
 * Receive a packet.
 */
static void rtl8139_receive_packet()
{
	struct rtl8139_private *tp = (struct rtl8139_private *) rtl8139_net_dev->private;
	struct rtl8139_rx_header *rx_header;
	struct sk_buff *skb;
	uint16_t rx_buf_ptr;

	/* handle all received packets */
	while ((inb(rtl8139_net_dev->io_base + ChipCmd) & 1) == 0) {
		/* get packet header */
		rx_buf_ptr = inw(rtl8139_net_dev->io_base + RxBufPtr) + 0x10;
		rx_header = (struct rtl8139_rx_header *) (tp->rx_buf + rx_buf_ptr);
		rx_buf_ptr = (rx_buf_ptr + rx_header->size + sizeof(struct rtl8139_rx_header) + 3) & ~3;

		/* allocate a socket buffer */
		skb = skb_alloc(rx_header->size);
		if (skb) {
			/* set network device */
			skb->dev = rtl8139_net_dev;

			/* copy data into socket buffer */
			skb_put(skb, rx_header->size);
			memcpy(skb->data, ((void *) rx_header) + sizeof(struct rtl8139_rx_header), rx_header->size);

			/* handle socket buffer */
			net_handle(skb);

			/* update stat */
			rtl8139_net_dev->stats.rx_packets++;
			rtl8139_net_dev->stats.rx_bytes += rx_header->size;
		}

		/* update received buffer pointer */
		outw(rtl8139_net_dev->io_base + RxBufPtr, rx_buf_ptr - 0x10);
	}
}

/*
 * Realtek 8139 IRQ handler.
 */
void rtl8139_irq_handler(struct registers *regs)
{
	int status;

	UNUSED(regs);

	/* get and ack status */
	status = inw(rtl8139_net_dev->io_base + IntrStatus);
	outw(rtl8139_net_dev->io_base + IntrStatus, status);

	/* handle reception */
	if (status & RxOK)
		rtl8139_receive_packet();
}

/*
 * Get Realtek 8139 network device.
 */
struct net_device *rtl8139_get_net_device()
{
	return rtl8139_net_dev;
}

static void rtl8139_init_ring()
{
	struct rtl8139_private *tp = (struct rtl8139_private *) rtl8139_net_dev->private;
	int i;

	tp->cur_tx = 0;

	for (i = 0; i < NUM_TX_DESC; i++)
		tp->tx_buf[i] = &tp->tx_bufs[i * TX_BUF_SIZE];
}

/*
 * Init Realtek 8139 device.
 */
int init_rtl8139()
{
	struct rtl8139_private *tp;
	struct pci_device *pci_dev;
	uint32_t io_base, pci_cmd;
	int i;

	/* get pci device */
	pci_dev = pci_get_device(RTL8139_VENDOR_ID, RTL8139_DEVICE_ID);
	if (!pci_dev)
		return -EINVAL;

	/* get I/O base address */
	io_base = pci_dev->bar0 & ~(0x3);

	/* register net device */
	rtl8139_net_dev = register_net_device(io_base, ARPHRD_ETHER);
	if (!rtl8139_net_dev)
		return -ENOSPC;

	/* allocate private data */
	rtl8139_net_dev->private = tp = (struct rtl8139_private *) kmalloc(sizeof(struct rtl8139_private));
	if (!tp)
		return -ENOMEM;

	/* get mac address */
	for (i = 0; i < 6; i++)
		rtl8139_net_dev->mac_addr[i] = inb(io_base + RTL8139_MAC_ADDRESS + i);

	/* set methods */
	rtl8139_net_dev->start_xmit = rtl8139_start_xmit;

	/* enable PCI Bus Mastering to allow NIC to perform DMA */
	pci_cmd = pci_read_field(pci_dev->address, PCI_CMD);
	if (!(pci_cmd & PCI_CMD_REG_BUS_MASTER)) {
		pci_cmd |= PCI_CMD_REG_BUS_MASTER;
		pci_write_field(pci_dev->address, PCI_CMD, pci_cmd);
	}

	/* power on the device */
	outb(io_base + Cfg9346, 0xC0);
	outb(io_base + Config1, 0x03);
	outb(io_base + HltClk, 'H');

	/* software reset (to clean up remaining buffers) */
	outb(io_base + ChipCmd, CmdReset);

	/* allocate receive/transmit buffers */
	tp->rx_buf = kmalloc(RX_BUF_SIZE);
	tp->tx_bufs = kmalloc(TX_BUF_SIZE * NUM_TX_DESC);
	if (!tp->rx_buf || !tp->tx_bufs) {
		if (tp->rx_buf)
			kfree(tp->rx_buf);
		return -ENOMEM;
	}
	rtl8139_init_ring();

	/* check that the chip has finished the reset. */
	for (i = 1000; i > 0; i--)
		if ((inb(io_base + ChipCmd) & CmdReset) == 0)
			break;

	/* memzero buffer and set physical address on chip */
	memset(tp->rx_buf, 0, RX_BUF_SIZE);
	outl(io_base + RxBuf, __pa(tp->rx_buf));

	/* set Interrupt Mask Register (only accept Transmit OK and Receive OK interrupts) */
	outw(io_base + IntrMask, PCIErr | PCSTimeout | RxUnderrun | RxOverflow | RxFIFOOver
		| TxErr | TxOK | RxErr | RxOK);

	/* configure receive buffer = accept Broadcast/Multicast/Pysical/All packets */
	outl(io_base + RxConfig, 0xF | (1 << 7));

	/* enable receive and transmitter (to accept and transmit packets) */
	outb(io_base + ChipCmd, CmdRxEnb | CmdTxEnb);

	/* get PCI interrupt line */
	rtl8139_net_dev->irq = pci_read_field(pci_dev->address, PCI_INTERRUPT_LINE);

	/* register interrupt handler */
	request_irq(rtl8139_net_dev->irq, rtl8139_irq_handler, "rtl8139");

	return 0;
}
