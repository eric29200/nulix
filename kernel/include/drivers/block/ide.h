#ifndef _IDE_H_
#define _IDE_H_

#include <drivers/block/genhd.h>
#include <fs/fs.h>
#include <stddef.h>

#define MAX_HWIFS			4
#define MAX_DRIVES			2

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

#define ATA_PRIMARY_IRQ			14
#define ATA_SECONDARY_IRQ		15

/*
 * ATA identification.
 */
struct ata_identify {
	uint16_t			flags;
	uint16_t			unused1[9];
	char				serial[20];
	uint16_t			unused2[3];
	char				firmware[8];
	char				model[40];
	uint16_t			sectors_per_int;
	uint16_t			unused3;
	uint16_t			capabilities[2];
	uint16_t			unused4[2];
	uint16_t			valid_ext_data;
	uint16_t			unused5[5];
	uint16_t			size_of_rw_mult;
	uint32_t			sectors_28;
	uint16_t			unused6[38];
	uint64_t			sectors_48;
	uint16_t			unused7[152];
} __attribute__((packed));

/*
 * ATA Physical Region Descriptor Table.
 */
struct ata_prdt {
	uint32_t			buffer_phys;
	uint16_t			transfert_size;
	uint16_t			mark_end;
} __attribute__((packed));

/*
 * IDE drive.
 */
struct ide_drive {
	int				id;
	char 				name[4];
	uint8_t				present:1;
	uint8_t				is_atapi:1;
	uint8_t				drive;
	uint16_t			io_base;
	struct ata_identify		identify;
	size_t				sector_size;
	struct partition *		part;
	struct ata_prdt *		prdt;
	uint8_t *			buf;
	uint32_t			bar4;
	int				(*read)(struct ide_drive *, uint32_t, size_t, char *);
	int				(*write)(struct ide_drive *, uint32_t, size_t, char *);
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
	uint8_t				present:1;
};

/* init functions */
int init_ide();
int ide_hd_init(struct ide_drive *drive);
int ide_cd_init(struct ide_drive *drive);

#endif
