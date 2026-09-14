#include <drivers/block/ide.h>
#include <x86/io.h>
#include <stderr.h>

/*
 * Setup dma.
 */
int ide_setup_dma(struct ide_drive *drive)
{
	int ret = -ENOMEM;

	/* set dma base address */
	drive->hwif->dma_base = drive->hwif->pci_dev->bar[4] & PCI_BASE_ADDRESS_IO_MASK;

	/* allocate prdt */
	drive->prdt = kmalloc(sizeof(struct ide_prdt));
	if (!drive->prdt)
		return -ENOMEM;

	/* allocate buffer */
	drive->buf = get_free_pages(ORDER_DMA_PAGES);
	if (!drive->buf)
		goto err;

	/* clear prdt and buffer */
	memset(drive->prdt, 0, sizeof(struct ide_prdt));
	memset(drive->buf, 0, NR_DMA_PAGES * PAGE_SIZE);

	/* set prdt */
	drive->prdt->buffer_phys = __pa(drive->buf);
	drive->prdt->mark_end = 0x8000;

	return 0;
err:
	kfree(drive->prdt);
	return ret;
}

/*
 * Issue a DMA command.
 */
void ide_dmaproc(struct ide_drive *drive, int cmd, size_t transfert_size)
{
	uint32_t dma_base = drive->hwif->dma_base;

	/* set transfert size */
	drive->prdt->transfert_size = transfert_size;

	/* prepare DMA transfert */
	outb(dma_base, 0);
	outl(dma_base + 0x04, __pa(drive->prdt));
	outb(dma_base + 0x02, inb(dma_base + 0x02) | 0x02 | 0x04);
	outb(dma_base, (cmd == ATA_CMD_READ_DMA ? 0x08 : 0x00) | 0x01);
}