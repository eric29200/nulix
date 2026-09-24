#include <drivers/block/ide.h>
#include <x86/io.h>
#include <stdio.h>
#include <stderr.h>

/*
 * Setup dma.
 */
int ide_setup_dma(struct ide_drive *drive)
{
	/* set dma base address */
	drive->hwif->dma_base = drive->hwif->pci_dev->bar[4] & PCI_BASE_ADDRESS_IO_MASK;

	/* allocate scatter list */
	drive->sg_table = (struct scatterlist *) kmalloc(sizeof(struct scatterlist) * PRD_ENTRIES);
	if (!drive->sg_table)
		return -ENOMEM;

	/* allocate dma table */
	drive->dma_table = get_free_pages(get_order(PRD_ENTRIES * PRD_BYTES));
	if (!drive->dma_table) {
		kfree(drive->sg_table);
		return -ENOMEM;
	}

	return 0;
}

/*
 * Build a scatter list with for dma.
 */
static int ide_build_sglist(struct ide_drive *drive, struct request *req)
{
	struct scatterlist *sg = drive->sg_table;
	uint32_t last_data_end = ~0UL;
	struct buffer_head *bh;
	struct list_head *pos;
	int nents = 0;

	/* add each buffer */
	list_for_each(pos, &req->bhs_list) {
		bh = list_entry(pos, struct buffer_head, b_list_req);

		/* merge with previous entry if possible */
		if ((uint32_t) bh->b_data == last_data_end) {
			sg[nents - 1].length += bh->b_size;
			last_data_end += bh->b_size;
			continue;
		}

		/* scatter list too small */
		if (nents >= PRD_ENTRIES)
			return 0;

		/* add buffer */
		memset(&sg[nents], 0, sizeof(struct scatterlist));
		sg[nents].address = bh->b_data;
		sg[nents].length = bh->b_size;
		nents++;

		/* save last data address */
		last_data_end = (uint32_t) bh->b_data + bh->b_size;
	}

	return nents;
}

/*
 * Build dma table.
 */
static int ide_build_dmatable(struct ide_drive *drive, struct request *req)
{
	uint32_t *table = drive->dma_table, cur_addr, cur_len, bcount;
	struct scatterlist *sg;
	int nents, count = 0;

	/* build scatter list */
	nents = ide_build_sglist(drive, req);
	if (!nents)
		return 0;

	/* build dma table, without crossing any 64kB boundaries */
	for (sg = drive->sg_table; sg->length && nents; sg++, nents--) {
		cur_addr = __pa(sg->address);
		cur_len = sg->length;

		while (cur_len) {
			if (count++ >= PRD_ENTRIES)
				return 0;

			/* limit dma entry to 64kB */
			bcount = 0x10000 - (cur_addr & 0xFFFF);
			if (bcount > cur_len)
				bcount = cur_len;

			/* set dma entry */
			*table++ = cur_addr;
			*table++ = bcount & 0xFFFF;

			cur_addr += bcount;
			cur_len -= bcount;
		}
	}

	/* end last entry */
	if (count)
		*--table |= 0x80000000;

	return count;
}

/*
 * Handle a dma interrupt.
 */
static void dma_irq_handler(struct ide_drive *drive)
{
	struct ide_hwif *hwif = drive->hwif;
	uint8_t stat, dma_stat;

	/* stop dma */
	outb(hwif->dma_base, inb(hwif->dma_base) & ~1);

	/* get status */
	dma_stat = ide_dmaproc(drive, NULL, ide_dma_end);
	stat = inb(HWIF(drive)->io_base + ATA_REG_STATUS);

	/* check status */
	if (!ATA_OK_STAT(stat, ATA_SR_DRDY, ATA_SR_ERR | ATA_SR_DRQ)) {
		printf("dma_irq_handler: bad status on drive %s : 0x%x\n", drive->name, stat);
		return;
	}

	/* check dma status */
	if (dma_stat) {
		printf("dma_irq_handler: bad DMA status on drive %s : 0x%x\n", drive->name, dma_stat);
		return;
	}

	/* end request */
	ide_end_request(hwif->hwgroup, 1);
}

/*
 * Issue a DMA command.
 */
int ide_dmaproc(struct ide_drive *drive, struct request *req, ide_dma_action_t func)
{
	uint32_t dma_base = HWIF(drive)->dma_base;
	uint8_t dma_stat;
	int reading = 0;

	switch (func) {
		case ide_dma_on:
			drive->using_dma = 1;
			return 0;
		case ide_dma_off:
			drive->using_dma = 0;
			return 0;
		case ide_dma_read:
			reading = 8;
			goto ide_dma_rw;
		case ide_dma_write:
ide_dma_rw:
			/* build dma table */
			if (!ide_build_dmatable(drive, req))
				return 1;

			/* prepare DMA transfert */
			outb(dma_base + 2, inb(dma_base + 2) | 6);
			outl(dma_base + 4, __pa(drive->dma_table));
			outb(dma_base, reading);

			/* issue command */
			HWGROUP(drive)->handler = &dma_irq_handler;
			outb(HWIF(drive)->io_base + ATA_REG_COMMAND, reading ? ATA_CMD_READ_DMA : ATA_CMD_WRITE_DMA);
			goto ide_dma_begin;
		case ide_dma_begin:
ide_dma_begin:
			outb(dma_base, inb(dma_base) | 1);
			return 0;
		case ide_dma_end:
			/* stop dma */
			outb(dma_base, inb(dma_base) & ~1);

			/* get status */
			dma_stat = inb(dma_base + 2);

			/* clear intr & error bits */
			outb(dma_base + 2, dma_stat | 6);

			/* return error/success */
			return (dma_stat & 7) != 4;
		default:
			printf("ide_dmaproc: unknown func %d\n", func);
			return 1;

	}
}