#include <drivers/block/ide.h>
#include <x86/io.h>
#include <stderr.h>

/*
 * Setup dma.
 */
int ide_setup_dma(struct ide_drive *drive)
{
	/* set dma base address */
	drive->hwif->dma_base = drive->hwif->pci_dev->bar[4] & PCI_BASE_ADDRESS_IO_MASK;

	/* allocate prdt */
	drive->prdt = kmalloc(sizeof(struct ide_prdt));
	if (!drive->prdt)
		return -ENOMEM;

	return 0;
}

/*
 * Issue a DMA command.
 */
void ide_dmaproc(struct ide_drive *drive, int cmd)
{
	uint32_t dma_base = drive->hwif->dma_base;

	/* prepare DMA transfert */
	outb(dma_base, 0);
	outl(dma_base + 0x04, __pa(drive->prdt));
	outb(dma_base + 0x02, inb(dma_base + 0x02) | 0x02 | 0x04);
	outb(dma_base, (cmd == ATA_CMD_READ_DMA ? 0x08 : 0x00) | 0x01);
	outb(drive->io_base + ATA_REG_COMMAND, cmd);
}