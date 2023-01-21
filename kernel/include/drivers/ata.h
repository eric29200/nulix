#ifndef _ATA_H_
#define _ATA_H_

#include <fs/fs.h>
#include <stddef.h>

#define NR_ATA_DEVICES			4

#define ATA_SECTOR_SIZE			512

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
 * ATA Physical Region Descriptor Table.
 */
struct ata_prdt_t {
	uint32_t		buffer_phys;
	uint16_t		transfert_size;
	uint16_t		mark_end;
} __attribute__((packed));

/*
 * ATA device.
 */
struct ata_device_t {
	uint8_t			bus;
	uint8_t			drive;
	uint16_t		io_base;
	struct ata_prdt_t	*prdt;
	uint8_t *		buf;
	uint32_t		bar4;
};

int init_ata();
struct ata_device_t *ata_get_device(dev_t dev);
int ata_read(dev_t dev, struct buffer_head_t *bh);
int ata_write(dev_t dev, struct buffer_head_t *bh);

extern struct inode_operations_t ata_iops;

#endif
