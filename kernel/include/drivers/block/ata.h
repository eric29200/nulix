#ifndef _ATA_H_
#define _ATA_H_

#include <drivers/block/genhd.h>
#include <fs/fs.h>
#include <stddef.h>

#define NR_ATA_DEVICES			4

#define ATA_SECTOR_SIZE			512
#define ATAPI_SECTOR_SIZE		2048

#define ATA_PRIMARY			0x00
#define ATA_SECONDARY			0x01
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

#define ATA_VENDOR_ID			0x8086
#define ATA_DEVICE_ID			0x7010

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
	uint32_t		buffer_phys;
	uint16_t		transfert_size;
	uint16_t		mark_end;
} __attribute__((packed));

/*
 * ATA device.
 */
struct ata_device {
	int			id;
	uint8_t			bus;
	uint8_t			drive;
	uint16_t		io_base;
	struct ata_identify	identify;
	char			is_atapi;
	size_t			sector_size;
	struct gendisk		hd;
	struct ata_prdt *	prdt;
	uint8_t *		buf;
	uint32_t		bar4;
	int			(*read)(struct ata_device *, uint32_t, size_t, char *);
	int			(*write)(struct ata_device *, uint32_t, size_t, char *);
};

/* init functions */
int init_ata();
int ata_hd_init(struct ata_device *device);
int ata_cd_init(struct ata_device *device);

extern struct inode_operations ata_iops;

#endif
