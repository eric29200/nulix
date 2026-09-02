#include <drivers/net/rtl8139.h>
#include <drivers/pci/pci.h>
#include <proc/sched.h>
#include <x86/interrupt.h>
#include <x86/io.h>
#include <net/inet/net.h>
#include <net/inet/ethernet.h>
#include <net/inet/arp.h>
#include <net/sk_buff.h>
#include <mm/paging.h>
#include <stdio.h>
#include <stderr.h>
#include <string.h>

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
static int rtl8139_start_xmit(struct sk_buff *skb, struct net_device *net_dev)
{
	struct rtl8139_private *tp = (struct rtl8139_private *) net_dev->private;

	/* copy packet to tx buffer */
	memcpy(tp->tx_buf[tp->cur_tx], skb->head, skb->len);

	/* put packet on device */
	outl(net_dev->io_base + TxAddr0 + tp->cur_tx * 4, __pa(tp->tx_buf[tp->cur_tx]));
	outl(net_dev->io_base + TxStatus0 + tp->cur_tx * 4, skb->size);

	/* update stats */
	net_dev->stats.tx_packets++;
	net_dev->stats.tx_bytes += skb->size;

	/* update tx buffer index */
	tp->cur_tx++;
	if (tp->cur_tx >= NUM_TX_DESC)
		tp->cur_tx = 0;

	return 0;
}

/*
 * Receive a packet.
 */
static void rtl8139_receive_packet(struct net_device *net_dev)
{
	struct rtl8139_private *tp = (struct rtl8139_private *) net_dev->private;
	struct rtl8139_rx_header *rx_header;
	struct sk_buff *skb;
	uint16_t rx_buf_ptr;

	/* handle all received packets */
	while ((inb(net_dev->io_base + ChipCmd) & 1) == 0) {
		/* get packet header */
		rx_buf_ptr = inw(net_dev->io_base + RxBufPtr) + 16;
		rx_header = (struct rtl8139_rx_header *) (tp->rx_buf + rx_buf_ptr);
		rx_buf_ptr = (rx_buf_ptr + rx_header->size + sizeof(struct rtl8139_rx_header) + 3) & ~3;

		/* allocate a socket buffer */
		skb = skb_alloc(rx_header->size);
		if (!skb) {
			net_dev->stats.rx_dropped++;
			break;
		}

		/* set network device */
		skb->dev = net_dev;

		/* copy data into socket buffer */
		skb_put(skb, rx_header->size);
		memcpy(skb->data, ((void *) rx_header) + sizeof(struct rtl8139_rx_header), rx_header->size);

		/* decode ethernet header */
		ethernet_receive(skb);

		/* receive packet */
		netif_rx(skb);

		/* update stat */
		net_dev->stats.rx_packets++;
		net_dev->stats.rx_bytes += rx_header->size;

		/* update received buffer pointer */
		outw(net_dev->io_base + RxBufPtr, rx_buf_ptr - 16);
	}
}

/*
 * Realtek 8139 IRQ handler.
 */
static void rtl8139_irq_handler(struct registers *regs, void *dev_instance)
{
	struct net_device *net_dev = (struct net_device *) dev_instance;
	int status;

	UNUSED(regs);

	/* get and ack status */
	status = inw(net_dev->io_base + IntrStatus);
	outw(net_dev->io_base + IntrStatus, status);

	/* handle reception */
	if (status & RxOK)
		rtl8139_receive_packet(net_dev);
}

/*
 * Init Realtek 8139 buffers.
 */
static void rtl8139_init_ring(struct net_device *net_dev)
{
	struct rtl8139_private *tp = (struct rtl8139_private *) net_dev->private;
	int i;

	tp->cur_tx = 0;

	for (i = 0; i < NUM_TX_DESC; i++)
		tp->tx_buf[i] = &tp->tx_bufs[i * TX_BUF_SIZE];
}

/*
 * Probe a Realtek 8139 device.
 */
static int rtl8139_probe(struct pci_device *pci_dev, struct pci_device_id *id)
{
	struct rtl8139_private *tp;
	struct net_device *net_dev;
	uint32_t io_base;
	int i;

	/* unused device id */
	UNUSED(id);

	/* get I/O base address */
	io_base = pci_dev->bar0 & ~(0x3);

	/* register net device */
	net_dev = register_net_device(io_base, ARPHRD_ETHER, AF_INET, "eth0");
	if (!net_dev)
		return -ENOSPC;

	/* allocate private data */
	net_dev->private = tp = (struct rtl8139_private *) kmalloc(sizeof(struct rtl8139_private));
	if (!tp)
		return -ENOMEM;

	/* get mac address */
	for (i = 0; i < ETHERNET_ALEN; i++)
		net_dev->hw_addr[i] = inb(io_base + RTL8139_MAC_ADDRESS + i);

	/* set device */
	net_dev->addr_len = ETHERNET_ALEN;
	net_dev->hard_header_len = ETHERNET_HLEN;
	net_dev->hard_header = ethernet_header;
	net_dev->rebuild_header = ethernet_rebuild_header;
	net_dev->start_xmit = rtl8139_start_xmit;

	/* enable pci device */
	pci_enable_device(pci_dev);
	pci_set_master(pci_dev);

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
	rtl8139_init_ring(net_dev);

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

	/* register interrupt handler */
	request_irq(pci_dev->irq, rtl8139_irq_handler, SA_SHIRQ, "rtl8139", net_dev);

	return 0;
}

/*
 * PCI ids table.
 */
static struct pci_device_id rtl8139_pci_tbl[] = {
	{ 0x10ec, 0x8139 },
	{ 0x10ec, 0x8138 },
	{ 0x1113, 0x1211 },
	{ 0x1500, 0x1360 },
	{ 0x4033, 0x1360 },
	{ 0x1186, 0x1300 },
	{ 0x1186, 0x1340 },
	{ 0x13d1, 0xab06 },
	{ 0x1259, 0xa117 },
	{ 0x1259, 0xa11e },
	{ 0x14ea, 0xab06 },
	{ 0x14ea, 0xab07 },
	{ 0x11db, 0x1234 },
	{ 0x1432, 0x9130 },
	{ 0x02ac, 0x1012 },
	{ 0x018a, 0x0106 },
	{ 0x126c, 0x1211 },
	{ 0x1743, 0x8139 },
	{ 0x021b, 0x8139 },
	{ 0, }
};

/*
 * PCI driver.
 */
static struct pci_driver rtl8139_pci_driver = {
	.id_table		= rtl8139_pci_tbl,
	.probe			= rtl8139_probe,
};

/*
 * Init Realtek 8139 devices.
 */
int init_rtl8139()
{
	int ret;

	/* register pci driver */
	ret = pci_register_driver(&rtl8139_pci_driver);
	if (ret > 0)
		return 0;

	return ret == 0 ? -ENODEV : ret;
}