#ifndef _LOOP_H_
#define _LOOP_H_

#include <fs/fs.h>

#define LO_FLAGS_DO_BMAP	0x00000001
#define LO_FLAGS_READ_ONLY	0x00000002

#define LOOP_SET_FD		0x4C00
#define LOOP_CLR_FD		0x4C01
#define LOOP_SET_STATUS		0x4C02
#define LOOP_GET_STATUS		0x4C03
#define LOOP_SET_STATUS64	0x4C04
#define LOOP_GET_STATUS64	0x4C05
#define LOOP_CHANGE_FD		0x4C06
#define LOOP_SET_CAPACITY	0x4C07
#define LOOP_SET_DIRECT_IO	0x4C08
#define LOOP_SET_BLOCK_SIZE	0x4C09
#define LOOP_CONFIGURE		0x4C0A

#define LO_NAME_SIZE		64
#define LO_KEY_SIZE		32

/*
 * Loop device.
 */
struct loop_device {
	int			lo_number;
	int			lo_count;
	struct dentry *		lo_dentry;
	dev_t			lo_device;
	int			lo_flags;
	off_t			lo_offset;
	struct file *		lo_backing_file;
	char			lo_name[LO_NAME_SIZE];
};

/*
 * Loop device informations.
 */
struct loop_info64 {
	uint64_t		lo_device;
	uint64_t		lo_inode;
	uint64_t		lo_rdevice;
	uint64_t		lo_offset;
	uint64_t		lo_sizelimit;
	uint32_t		lo_number;
	uint32_t		lo_encrypt_type;
	uint32_t		lo_encrypt_key_size;
	uint32_t		lo_flags;
	uint8_t			lo_file_name[LO_NAME_SIZE];
	uint8_t			lo_crypt_name[LO_NAME_SIZE];
	uint8_t			lo_encrypt_key[LO_KEY_SIZE];
	uint64_t		lo_init[2];
};

/*
 * Loop configuration.
 */
struct loop_config {
	uint32_t		fd;
	uint32_t		block_size;
	struct loop_info64	info;
	uint64_t		__reserved[8];
};

int init_loop();

#endif