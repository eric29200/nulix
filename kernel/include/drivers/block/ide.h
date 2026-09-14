#ifndef _IDE_H_
#define _IDE_H_

#include <drivers/block/genhd.h>
#include <drivers/block/blk_dev.h>
#include <drivers/pci/pci.h>
#include <fs/fs.h>
#include <stddef.h>

#define IDE_ORDER_DMA_PAGES		2
#define IDE_NR_DMA_PAGES		(1 << (IDE_ORDER_DMA_PAGES))

#define MAX_HWIFS			4
#define MAX_DRIVES			2

#define IDE_DISK			0x20
#define IDE_CDROM			0x05

#define ATA_SECTOR_SIZE			512
#define ATAPI_SECTOR_SIZE		2048

#define ATA_PRIMARY_IO			0x1F0
#define ATA_SECONDARY_IO		0x170
#define ATA_MASTER			0x00
#define ATA_SLAVE			0x01

/* ATA registers */
#define ATA_REG_DATA			0x00
#define ATA_REG_ERROR			0x01
#define ATA_REG_FEATURES		0x01
#define ATA_REG_SECCOUNT0		0x02
#define ATA_REG_LBA0			0x03
#define ATA_REG_LBA1			0x04
#define ATA_REG_LBA2			0x05
#define ATA_REG_HDDEVSEL		0x06
#define ATA_REG_STATUS			0x07
#define ATA_REG_COMMAND			0x07
#define ATA_REG_ALTSTATUS		0x0C
#define ATA_REG_CONTROL			0x0C

/* ATA commands */
#define ATA_CMD_READ_PIO		0x20
#define ATA_CMD_READ_PIO_EXT		0x24
#define ATA_CMD_READ_DMA		0xC8
#define ATA_CMD_READ_DMA_EXT		0x25
#define ATA_CMD_WRITE_PIO		0x30
#define ATA_CMD_WRITE_PIO_EXT		0x34
#define ATA_CMD_WRITE_DMA		0xCA
#define ATA_CMD_WRITE_DMA_EXT		0x35
#define ATA_CMD_CACHE_FLUSH		0xE7
#define ATA_CMD_CACHE_FLUSH_EXT		0xEA
#define ATA_CMD_PACKET			0xA0
#define ATA_CMD_IDENTIFY_PACKET		0xA1
#define ATA_CMD_IDENTIFY		0xEC

/* ATA Status Register */
#define ATA_SR_BSY			0x80
#define ATA_SR_DRDY			0x40
#define ATA_SR_DF			0x20
#define ATA_SR_DSC			0x10
#define ATA_SR_DRQ			0x08
#define ATA_SR_CORR			0x04
#define ATA_SR_IDX			0x02
#define ATA_SR_ERR			0x01

/* ATA Errors */
#define ATA_ER_BBK			0x80
#define ATA_ER_UNC			0x40
#define ATA_ER_MC			0x20
#define ATA_ER_IDNF			0x10
#define ATA_ER_MCR			0x08
#define ATA_ER_ABRT			0x04
#define ATA_ER_TK0NF			0x02
#define ATA_ER_AMNF			0x01

/*
 * IDE identification.
 */
struct hd_driveid {
	uint16_t			config;		/* lots of obsolete bit flags */
	uint16_t			cyls;		/* "physical" cyls */
	uint16_t			reserved2;	/* reserved (word 2) */
	uint16_t			heads;		/* "physical" heads */
	uint16_t			track_bytes;	/* unformatted bytes per track */
	uint16_t			sector_bytes;	/* unformatted bytes per sector */
	uint16_t			sectors;	/* "physical" sectors per track */
	uint16_t			vendor0;	/* vendor unique */
	uint16_t			vendor1;	/* vendor unique */
	uint16_t			vendor2;	/* vendor unique */
	uint8_t				serial_no[20];	/* 0 = not_specified */
	uint16_t			buf_type;
	uint16_t			buf_size;	/* 512 byte increments; 0 = not_specified */
	uint16_t			ecc_bytes;	/* for r/w long cmds; 0 = not_specified */
	uint8_t				fw_rev[8];	/* 0 = not_specified */
	uint8_t				model[40];	/* 0 = not_specified */
	uint8_t				max_multsect;	/* 0=not_implemented */
	uint8_t				vendor3;	/* vendor unique */
	uint16_t			dword_io;	/* 0=not_implemented; 1=implemented */
	uint8_t				vendor4;	/* vendor unique */
	uint8_t				capability;	/* bits 0:DMA 1:LBA 2:IORDYsw 3:IORDYsup*/
	uint16_t			reserved50;	/* reserved (word 50) */
	uint8_t				vendor5;	/* vendor unique */
	uint8_t				tPIO;		/* 0=slow, 1=medium, 2=fast */
	uint8_t				vendor6;	/* vendor unique */
	uint8_t				tDMA;		/* 0=slow, 1=medium, 2=fast */
	uint16_t			field_valid;	/* bits 0:cur_ok 1:eide_ok */
	uint16_t			cur_cyls;	/* logical cylinders */
	uint16_t			cur_heads;	/* logical heads */
	uint16_t			cur_sectors;	/* logical sectors per track */
	uint16_t			cur_capacity0;	/* logical total sectors on drive */
	uint16_t			cur_capacity1;	/*  (2 words, misaligned int)     */
	uint8_t				multsect;	/* current multiple sector count */
	uint8_t				multsect_valid;	/* when (bit0==1) multsect is ok */
	uint32_t			lba_capacity;	/* total number of sectors */
	uint16_t			dma_1word;	/* single-word dma info */
	uint16_t			dma_mword;	/* multiple-word dma info */
	uint16_t  			eide_pio_modes; /* bits 0:mode3 1:mode4 */
	uint16_t 			eide_dma_min;	/* min mword dma cycle time (ns) */
	uint16_t  			eide_dma_time;	/* recommended mword dma cycle time (ns) */
	uint16_t  			eide_pio;       /* min cycle time (ns), no IORDY  */
	uint16_t  			eide_pio_iordy; /* min cycle time (ns), with IORDY */
	uint16_t  			word69;
	uint16_t  			word70;
	uint16_t  			word71;
	uint16_t  			word72;
	uint16_t  			word73;
	uint16_t  			word74;
	uint16_t  			word75;
	uint16_t  			word76;
	uint16_t  			word77;
	uint16_t  			word78;
	uint16_t  			word79;
	uint16_t  			word80;
	uint16_t  			word81;
	uint16_t  			command_sets;	/* bits 0:Smart 1:Security 2:Removable 3:PM */
	uint16_t  			word83;		/* bits 14:Smart Enabled 13:0 zero */
	uint16_t  			word84;
	uint16_t  			word85;
	uint16_t  			word86;
	uint16_t  			word87;
	uint16_t 		 	dma_ultra;
	uint16_t			word89;		/* reserved (word 89) */
	uint16_t			word90;		/* reserved (word 90) */
	uint16_t			word91;		/* reserved (word 91) */
	uint16_t			word92;		/* reserved (word 92) */
	uint16_t			word93;		/* reserved (word 93) */
	uint16_t			word94;		/* reserved (word 94) */
	uint16_t			word95;		/* reserved (word 95) */
	uint16_t			word96;		/* reserved (word 96) */
	uint16_t			word97;		/* reserved (word 97) */
	uint16_t			word98;		/* reserved (word 98) */
	uint16_t			word99;		/* reserved (word 99) */
	uint16_t			word100;	/* reserved (word 100) */
	uint16_t			word101;	/* reserved (word 101) */
	uint16_t			word102;	/* reserved (word 102) */
	uint16_t			word103;	/* reserved (word 103) */
	uint16_t			word104;	/* reserved (word 104) */
	uint16_t			word105;	/* reserved (word 105) */
	uint16_t			word106;	/* reserved (word 106) */
	uint16_t			word107;	/* reserved (word 107) */
	uint16_t			word108;	/* reserved (word 108) */
	uint16_t			word109;	/* reserved (word 109) */
	uint16_t			word110;	/* reserved (word 110) */
	uint16_t			word111;	/* reserved (word 111) */
	uint16_t			word112;	/* reserved (word 112) */
	uint16_t			word113;	/* reserved (word 113) */
	uint16_t			word114;	/* reserved (word 114) */
	uint16_t			word115;	/* reserved (word 115) */
	uint16_t			word116;	/* reserved (word 116) */
	uint16_t			word117;	/* reserved (word 117) */
	uint16_t			word118;	/* reserved (word 118) */
	uint16_t			word119;	/* reserved (word 119) */
	uint16_t			word120;	/* reserved (word 120) */
	uint16_t			word121;	/* reserved (word 121) */
	uint16_t			word122;	/* reserved (word 122) */
	uint16_t			word123;	/* reserved (word 123) */
	uint16_t			word124;	/* reserved (word 124) */
	uint16_t			word125;	/* reserved (word 125) */
	uint16_t			word126;	/* reserved (word 126) */
	uint16_t			word127;	/* reserved (word 127) */
	uint16_t			security;	/* bits 0:suuport 1:enabled 2:locked 3:frozen */
	uint16_t			reserved[127];
} __attribute__((packed));

/*
 * IDE Physical Region Descriptor Table.
 */
struct ide_prdt {
	uint32_t			buffer_phys;
	uint16_t			transfert_size;
	uint16_t			mark_end;
} __attribute__((packed));

/*
 * IDE drive.
 */
struct ide_drive {
	char 				name[4];
	uint8_t				present:1;
	uint8_t				media;
	uint8_t				drive;
	uint16_t			io_base;
	struct hd_driveid *		id;
	struct partition *		part;
	struct ide_hwif *		hwif;
	struct ide_prdt *		prdt;
	uint8_t *			buf;
};

/*
 * IDE interface.
 */
struct ide_hwif {
	struct ide_drive		drives[MAX_DRIVES];
	uint8_t				major;
	char 				name[5];
	uint8_t				index;
	struct gendisk *		gd;
	struct pci_device *		pci_dev;
	uint32_t			dma_base;
	uint8_t				present:1;
};

/* init functions */
int init_ide();
int ide_setup_dma(struct ide_drive *drive);
void ide_dmaproc(struct ide_drive *drive, int cmd, size_t transfert_size);
int ide_do_rw_disk(struct ide_drive *drive, struct request *req);
int ide_do_rw_cdrom(struct ide_drive *drive, struct request *req);

#endif
